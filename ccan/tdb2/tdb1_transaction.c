 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              2005

     ** NOTE! The following LGPL license applies to the tdb
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

#include "tdb1_private.h"

/*
  transaction design:

  - only allow a single transaction at a time per database. This makes
    using the transaction API simpler, as otherwise the caller would
    have to cope with temporary failures in transactions that conflict
    with other current transactions

  - keep the transaction recovery information in the same file as the
    database, using a special 'transaction recovery' record pointed at
    by the header. This removes the need for extra journal files as
    used by some other databases

  - dynamically allocated the transaction recover record, re-using it
    for subsequent transactions. If a larger record is needed then
    tdb1_free() the old record to place it on the normal tdb freelist
    before allocating the new record

  - during transactions, keep a linked list of writes all that have
    been performed by intercepting all tdb1_write() calls. The hooked
    transaction versions of tdb1_read() and tdb1_write() check this
    linked list and try to use the elements of the list in preference
    to the real database.

  - don't allow any locks to be held when a transaction starts,
    otherwise we can end up with deadlock (plus lack of lock nesting
    in posix locks would mean the lock is lost)

  - if the caller gains a lock during the transaction but doesn't
    release it then fail the commit

  - allow for nested calls to tdb1_transaction_start(), re-using the
    existing transaction record. If the inner transaction is cancelled
    then a subsequent commit will fail

  - keep a mirrored copy of the tdb hash chain heads to allow for the
    fast hash heads scan on traverse, updating the mirrored copy in
    the transaction version of tdb1_write

  - allow callers to mix transaction and non-transaction use of tdb,
    although once a transaction is started then an exclusive lock is
    gained until the transaction is committed or cancelled

  - the commit stategy involves first saving away all modified data
    into a linearised buffer in the transaction recovery area, then
    marking the transaction recovery area with a magic value to
    indicate a valid recovery record. In total 4 fsync/msync calls are
    needed per commit to prevent race conditions. It might be possible
    to reduce this to 3 or even 2 with some more work.

  - check for a valid recovery record on open of the tdb, while the
    open lock is held. Automatically recover from the transaction
    recovery area if needed, then continue with the open as
    usual. This allows for smooth crash recovery with no administrator
    intervention.

  - if TDB_NOSYNC is passed to flags in tdb1_open then transactions are
    still available, but no transaction recovery area is used and no
    fsync/msync calls are made.

  - if TDB_ALLOW_NESTING is passed to flags in tdb open, or added using
    tdb1_add_flags() transaction nesting is enabled.
    The default is that transaction nesting is NOT allowed.

    Beware. when transactions are nested a transaction successfully
    completed with tdb1_transaction_commit() can be silently unrolled later.
*/


/*
  hold the context of any current transaction
*/
struct tdb1_transaction {
	/* we keep a mirrored copy of the tdb hash heads here so
	   tdb1_next_hash_chain() can operate efficiently */
	uint32_t *hash_heads;

	/* the original io methods - used to do IOs to the real db */
	const struct tdb1_methods *io_methods;

	/* the list of transaction blocks. When a block is first
	   written to, it gets created in this list */
	uint8_t **blocks;
	uint32_t num_blocks;
	uint32_t block_size;      /* bytes in each block */
	uint32_t last_block_size; /* number of valid bytes in the last block */

	/* non-zero when an internal transaction error has
	   occurred. All write operations will then fail until the
	   transaction is ended */
	int transaction_error;

	/* when inside a transaction we need to keep track of any
	   nested tdb1_transaction_start() calls, as these are allowed,
	   but don't create a new transaction */
	int nesting;

	/* set when a prepare has already occurred */
	bool prepared;
	tdb1_off_t magic_offset;

	/* old file size before transaction */
	tdb1_len_t old_map_size;

	/* did we expand in this transaction */
	bool expanded;
};


/*
  read while in a transaction. We need to check first if the data is in our list
  of transaction elements, then if not do a real read
*/
static int transaction1_read(struct tdb1_context *tdb, tdb1_off_t off, void *buf,
			     tdb1_len_t len, int cv)
{
	uint32_t blk;

	/* break it down into block sized ops */
	while (len + (off % tdb->transaction->block_size) > tdb->transaction->block_size) {
		tdb1_len_t len2 = tdb->transaction->block_size - (off % tdb->transaction->block_size);
		if (transaction1_read(tdb, off, buf, len2, cv) != 0) {
			return -1;
		}
		len -= len2;
		off += len2;
		buf = (void *)(len2 + (char *)buf);
	}

	if (len == 0) {
		return 0;
	}

	blk = off / tdb->transaction->block_size;

	/* see if we have it in the block list */
	if (tdb->transaction->num_blocks <= blk ||
	    tdb->transaction->blocks[blk] == NULL) {
		/* nope, do a real read */
		if (tdb->transaction->io_methods->tdb1_read(tdb, off, buf, len, cv) != 0) {
			goto fail;
		}
		return 0;
	}

	/* it is in the block list. Now check for the last block */
	if (blk == tdb->transaction->num_blocks-1) {
		if (len > tdb->transaction->last_block_size) {
			goto fail;
		}
	}

	/* now copy it out of this block */
	memcpy(buf, tdb->transaction->blocks[blk] + (off % tdb->transaction->block_size), len);
	if (cv) {
		tdb1_convert(buf, len);
	}
	return 0;

fail:
	tdb->last_error = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				"transaction_read: failed at off=%d len=%d",
				off, len);
	tdb->transaction->transaction_error = 1;
	return -1;
}


/*
  write while in a transaction
*/
static int transaction1_write(struct tdb1_context *tdb, tdb1_off_t off,
			     const void *buf, tdb1_len_t len)
{
	uint32_t blk;

	/* Only a commit is allowed on a prepared transaction */
	if (tdb->transaction->prepared) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"transaction_write: transaction already"
					" prepared, write not allowed");
		tdb->transaction->transaction_error = 1;
		return -1;
	}

	/* if the write is to a hash head, then update the transaction
	   hash heads */
	if (len == sizeof(tdb1_off_t) && off >= TDB1_FREELIST_TOP &&
	    off < TDB1_FREELIST_TOP+TDB1_HASHTABLE_SIZE(tdb)) {
		uint32_t chain = (off-TDB1_FREELIST_TOP) / sizeof(tdb1_off_t);
		memcpy(&tdb->transaction->hash_heads[chain], buf, len);
	}

	/* break it up into block sized chunks */
	while (len + (off % tdb->transaction->block_size) > tdb->transaction->block_size) {
		tdb1_len_t len2 = tdb->transaction->block_size - (off % tdb->transaction->block_size);
		if (transaction1_write(tdb, off, buf, len2) != 0) {
			return -1;
		}
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return 0;
	}

	blk = off / tdb->transaction->block_size;
	off = off % tdb->transaction->block_size;

	if (tdb->transaction->num_blocks <= blk) {
		uint8_t **new_blocks;
		/* expand the blocks array */
		if (tdb->transaction->blocks == NULL) {
			new_blocks = (uint8_t **)malloc(
				(blk+1)*sizeof(uint8_t *));
		} else {
			new_blocks = (uint8_t **)realloc(
				tdb->transaction->blocks,
				(blk+1)*sizeof(uint8_t *));
		}
		if (new_blocks == NULL) {
			tdb->last_error = TDB_ERR_OOM;
			goto fail;
		}
		memset(&new_blocks[tdb->transaction->num_blocks], 0,
		       (1+(blk - tdb->transaction->num_blocks))*sizeof(uint8_t *));
		tdb->transaction->blocks = new_blocks;
		tdb->transaction->num_blocks = blk+1;
		tdb->transaction->last_block_size = 0;
	}

	/* allocate and fill a block? */
	if (tdb->transaction->blocks[blk] == NULL) {
		tdb->transaction->blocks[blk] = (uint8_t *)calloc(tdb->transaction->block_size, 1);
		if (tdb->transaction->blocks[blk] == NULL) {
			tdb->last_error = TDB_ERR_OOM;
			tdb->transaction->transaction_error = 1;
			return -1;
		}
		if (tdb->transaction->old_map_size > blk * tdb->transaction->block_size) {
			tdb1_len_t len2 = tdb->transaction->block_size;
			if (len2 + (blk * tdb->transaction->block_size) > tdb->transaction->old_map_size) {
				len2 = tdb->transaction->old_map_size - (blk * tdb->transaction->block_size);
			}
			if (tdb->transaction->io_methods->tdb1_read(tdb, blk * tdb->transaction->block_size,
								   tdb->transaction->blocks[blk],
								   len2, 0) != 0) {
				SAFE_FREE(tdb->transaction->blocks[blk]);
				tdb->last_error = TDB_ERR_IO;
				goto fail;
			}
			if (blk == tdb->transaction->num_blocks-1) {
				tdb->transaction->last_block_size = len2;
			}
		}
	}

	/* overwrite part of an existing block */
	if (buf == NULL) {
		memset(tdb->transaction->blocks[blk] + off, 0, len);
	} else {
		memcpy(tdb->transaction->blocks[blk] + off, buf, len);
	}
	if (blk == tdb->transaction->num_blocks-1) {
		if (len + off > tdb->transaction->last_block_size) {
			tdb->transaction->last_block_size = len + off;
		}
	}

	return 0;

fail:
	tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
		   "transaction_write: failed at off=%d len=%d",
		   (blk*tdb->transaction->block_size) + off, len);
	tdb->transaction->transaction_error = 1;
	return -1;
}


/*
  write while in a transaction - this varient never expands the transaction blocks, it only
  updates existing blocks. This means it cannot change the recovery size
*/
static int transaction1_write_existing(struct tdb1_context *tdb, tdb1_off_t off,
				      const void *buf, tdb1_len_t len)
{
	uint32_t blk;

	/* break it up into block sized chunks */
	while (len + (off % tdb->transaction->block_size) > tdb->transaction->block_size) {
		tdb1_len_t len2 = tdb->transaction->block_size - (off % tdb->transaction->block_size);
		if (transaction1_write_existing(tdb, off, buf, len2) != 0) {
			return -1;
		}
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return 0;
	}

	blk = off / tdb->transaction->block_size;
	off = off % tdb->transaction->block_size;

	if (tdb->transaction->num_blocks <= blk ||
	    tdb->transaction->blocks[blk] == NULL) {
		return 0;
	}

	if (blk == tdb->transaction->num_blocks-1 &&
	    off + len > tdb->transaction->last_block_size) {
		if (off >= tdb->transaction->last_block_size) {
			return 0;
		}
		len = tdb->transaction->last_block_size - off;
	}

	/* overwrite part of an existing block */
	memcpy(tdb->transaction->blocks[blk] + off, buf, len);

	return 0;
}


/*
  accelerated hash chain head search, using the cached hash heads
*/
static void transaction1_next_hash_chain(struct tdb1_context *tdb, uint32_t *chain)
{
	uint32_t h = *chain;
	for (;h < tdb->header.hash_size;h++) {
		/* the +1 takes account of the freelist */
		if (0 != tdb->transaction->hash_heads[h+1]) {
			break;
		}
	}
	(*chain) = h;
}

/*
  out of bounds check during a transaction
*/
static int transaction1_oob(struct tdb1_context *tdb, tdb1_off_t len, int probe)
{
	if (len <= tdb->file->map_size) {
		return 0;
	}
	tdb->last_error = TDB_ERR_IO;
	return -1;
}

/*
  transaction version of tdb1_expand().
*/
static int transaction1_expand_file(struct tdb1_context *tdb, tdb1_off_t size,
				    tdb1_off_t addition)
{
	/* add a write to the transaction elements, so subsequent
	   reads see the zero data */
	if (transaction1_write(tdb, size, NULL, addition) != 0) {
		return -1;
	}

	tdb->transaction->expanded = true;

	return 0;
}

static const struct tdb1_methods transaction1_methods = {
	transaction1_read,
	transaction1_write,
	transaction1_next_hash_chain,
	transaction1_oob,
	transaction1_expand_file,
};


/*
  start a tdb transaction. No token is returned, as only a single
  transaction is allowed to be pending per tdb1_context
*/
static int _tdb1_transaction_start(struct tdb1_context *tdb)
{
	/* some sanity checks */
	if (tdb->read_only || (tdb->flags & TDB_INTERNAL) || tdb->traverse_read) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"tdb1_transaction_start: cannot start a"
					" transaction on a read-only or"
					" internal db");
		return -1;
	}

	/* cope with nested tdb1_transaction_start() calls */
	if (tdb->transaction != NULL) {
		if (!(tdb->flags & TDB_ALLOW_NESTING)) {
			tdb->last_error = TDB_ERR_EINVAL;
			return -1;
		}
		tdb->transaction->nesting++;
		return 0;
	}

	if (tdb1_have_extra_locks(tdb)) {
		/* the caller must not have any locks when starting a
		   transaction as otherwise we'll be screwed by lack
		   of nested locks in posix */
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
					"tdb1_transaction_start: cannot start a"
					" transaction with locks held");
		return -1;
	}

	if (tdb->travlocks.next != NULL) {
		/* you cannot use transactions inside a traverse (although you can use
		   traverse inside a transaction) as otherwise you can end up with
		   deadlock */
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
					"tdb1_transaction_start: cannot start a"
					" transaction within a traverse");
		return -1;
	}

	tdb->transaction = (struct tdb1_transaction *)
		calloc(sizeof(struct tdb1_transaction), 1);
	if (tdb->transaction == NULL) {
		tdb->last_error = TDB_ERR_OOM;
		return -1;
	}

	/* a page at a time seems like a reasonable compromise between compactness and efficiency */
	tdb->transaction->block_size = tdb->page_size;

	/* get the transaction write lock. This is a blocking lock. As
	   discussed with Volker, there are a number of ways we could
	   make this async, which we will probably do in the future */
	if (tdb1_transaction_lock(tdb, F_WRLCK, TDB_LOCK_WAIT) == -1) {
		SAFE_FREE(tdb->transaction->blocks);
		SAFE_FREE(tdb->transaction);
		return -1;
	}

	/* get a read lock from the freelist to the end of file. This
	   is upgraded to a write lock during the commit */
	if (tdb1_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, true) == -1) {
		if (errno != EAGAIN && errno != EINTR) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_start:"
				   " failed to get hash locks");
		}
		goto fail_allrecord_lock;
	}

	/* setup a copy of the hash table heads so the hash scan in
	   traverse can be fast */
	tdb->transaction->hash_heads = (uint32_t *)
		calloc(tdb->header.hash_size+1, sizeof(uint32_t));
	if (tdb->transaction->hash_heads == NULL) {
		tdb->last_error = TDB_ERR_OOM;
		goto fail;
	}
	if (tdb->methods->tdb1_read(tdb, TDB1_FREELIST_TOP, tdb->transaction->hash_heads,
				   TDB1_HASHTABLE_SIZE(tdb), 0) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_start: failed to read hash heads");
		goto fail;
	}

	/* make sure we know about any file expansions already done by
	   anyone else */
	tdb->methods->tdb1_oob(tdb, tdb->file->map_size + 1, 1);
	tdb->transaction->old_map_size = tdb->file->map_size;

	/* finally hook the io methods, replacing them with
	   transaction specific methods */
	tdb->transaction->io_methods = tdb->methods;
	tdb->methods = &transaction1_methods;

	return 0;

fail:
	tdb1_allrecord_unlock(tdb, F_RDLCK);
fail_allrecord_lock:
	tdb1_transaction_unlock(tdb, F_WRLCK);
	SAFE_FREE(tdb->transaction->blocks);
	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);
	return -1;
}

int tdb1_transaction_start(struct tdb1_context *tdb)
{
	return _tdb1_transaction_start(tdb);
}

/*
  sync to disk
*/
static int transaction1_sync(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_len_t length)
{
	if (tdb->flags & TDB_NOSYNC) {
		return 0;
	}

#if HAVE_FDATASYNC
	if (fdatasync(tdb->file->fd) != 0) {
#else
	if (fsync(tdb->file->fd) != 0) {
#endif
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					"tdb1_transaction: fsync failed");
		return -1;
	}
#if HAVE_MMAP
	if (tdb->file->map_ptr) {
		tdb1_off_t moffset = offset & ~(tdb->page_size-1);
		if (msync(moffset + (char *)tdb->file->map_ptr,
			  length + (offset - moffset), MS_SYNC) != 0) {
			tdb->last_error = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
						"tdb1_transaction:"
						" msync failed - %s",
						strerror(errno));
			return -1;
		}
	}
#endif
	return 0;
}


static int _tdb1_transaction_cancel(struct tdb1_context *tdb)
{
	int i, ret = 0;

	if (tdb->transaction == NULL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"tdb1_transaction_cancel:"
					" no transaction");
		return -1;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->transaction_error = 1;
		tdb->transaction->nesting--;
		return 0;
	}

	tdb->file->map_size = tdb->transaction->old_map_size;

	/* free all the transaction blocks */
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		if (tdb->transaction->blocks[i] != NULL) {
			free(tdb->transaction->blocks[i]);
		}
	}
	SAFE_FREE(tdb->transaction->blocks);

	if (tdb->transaction->magic_offset) {
		const struct tdb1_methods *methods = tdb->transaction->io_methods;
		const uint32_t invalid = TDB1_RECOVERY_INVALID_MAGIC;

		/* remove the recovery marker */
		if (methods->tdb1_write(tdb, tdb->transaction->magic_offset, &invalid, 4) == -1 ||
		transaction1_sync(tdb, tdb->transaction->magic_offset, 4) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_cancel: failed to"
				   " remove recovery magic");
			ret = -1;
		}
	}

	/* This also removes the OPEN_LOCK, if we have it. */
	tdb1_release_transaction_locks(tdb);

	/* restore the normal io methods */
	tdb->methods = tdb->transaction->io_methods;

	SAFE_FREE(tdb->transaction->hash_heads);
	SAFE_FREE(tdb->transaction);

	return ret;
}

/*
  cancel the current transaction
*/
int tdb1_transaction_cancel(struct tdb1_context *tdb)
{
	return _tdb1_transaction_cancel(tdb);
}

/*
  work out how much space the linearised recovery data will consume
*/
static tdb1_len_t tdb1_recovery_size(struct tdb1_context *tdb)
{
	tdb1_len_t recovery_size = 0;
	int i;

	recovery_size = sizeof(uint32_t);
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		if (i * tdb->transaction->block_size >= tdb->transaction->old_map_size) {
			break;
		}
		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}
		recovery_size += 2*sizeof(tdb1_off_t);
		if (i == tdb->transaction->num_blocks-1) {
			recovery_size += tdb->transaction->last_block_size;
		} else {
			recovery_size += tdb->transaction->block_size;
		}
	}

	return recovery_size;
}

int tdb1_recovery_area(struct tdb1_context *tdb,
		      const struct tdb1_methods *methods,
		      tdb1_off_t *recovery_offset,
		      struct tdb1_record *rec)
{
	if (tdb1_ofs_read(tdb, TDB1_RECOVERY_HEAD, recovery_offset) == -1) {
		return -1;
	}

	if (*recovery_offset == 0) {
		rec->rec_len = 0;
		return 0;
	}

	if (methods->tdb1_read(tdb, *recovery_offset, rec, sizeof(*rec),
			      TDB1_DOCONV()) == -1) {
		return -1;
	}

	/* ignore invalid recovery regions: can happen in crash */
	if (rec->magic != TDB1_RECOVERY_MAGIC &&
	    rec->magic != TDB1_RECOVERY_INVALID_MAGIC) {
		*recovery_offset = 0;
		rec->rec_len = 0;
	}
	return 0;
}

/*
  allocate the recovery area, or use an existing recovery area if it is
  large enough
*/
static int tdb1_recovery_allocate(struct tdb1_context *tdb,
				 tdb1_len_t *recovery_size,
				 tdb1_off_t *recovery_offset,
				 tdb1_len_t *recovery_max_size)
{
	struct tdb1_record rec;
	const struct tdb1_methods *methods = tdb->transaction->io_methods;
	tdb1_off_t recovery_head;

	if (tdb1_recovery_area(tdb, methods, &recovery_head, &rec) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_recovery_allocate:"
			   " failed to read recovery head");
		return -1;
	}

	*recovery_size = tdb1_recovery_size(tdb);

	if (recovery_head != 0 && *recovery_size <= rec.rec_len) {
		/* it fits in the existing area */
		*recovery_max_size = rec.rec_len;
		*recovery_offset = recovery_head;
		return 0;
	}

	/* we need to free up the old recovery area, then allocate a
	   new one at the end of the file. Note that we cannot use
	   tdb1_allocate() to allocate the new one as that might return
	   us an area that is being currently used (as of the start of
	   the transaction) */
	if (recovery_head != 0) {
		if (tdb1_free(tdb, recovery_head, &rec) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_recovery_allocate: failed to free"
				   " previous recovery area");
			return -1;
		}
	}

	/* the tdb1_free() call might have increased the recovery size */
	*recovery_size = tdb1_recovery_size(tdb);

	/* round up to a multiple of page size */
	*recovery_max_size = TDB1_ALIGN(sizeof(rec) + *recovery_size, tdb->page_size) - sizeof(rec);
	*recovery_offset = tdb->file->map_size;
	recovery_head = *recovery_offset;

	if (methods->tdb1_expand_file(tdb, tdb->transaction->old_map_size,
				     (tdb->file->map_size - tdb->transaction->old_map_size) +
				     sizeof(rec) + *recovery_max_size) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_recovery_allocate:"
			   " failed to create recovery area");
		return -1;
	}

	/* remap the file (if using mmap) */
	methods->tdb1_oob(tdb, tdb->file->map_size + 1, 1);

	/* we have to reset the old map size so that we don't try to expand the file
	   again in the transaction commit, which would destroy the recovery area */
	tdb->transaction->old_map_size = tdb->file->map_size;

	/* write the recovery header offset and sync - we can sync without a race here
	   as the magic ptr in the recovery record has not been set */
	TDB1_CONV(recovery_head);
	if (methods->tdb1_write(tdb, TDB1_RECOVERY_HEAD,
			       &recovery_head, sizeof(tdb1_off_t)) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_recovery_allocate:"
			   " failed to write recovery head");
		return -1;
	}
	if (transaction1_write_existing(tdb, TDB1_RECOVERY_HEAD, &recovery_head, sizeof(tdb1_off_t)) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_recovery_allocate:"
			   " failed to write recovery head");
		return -1;
	}

	return 0;
}


/*
  setup the recovery data that will be used on a crash during commit
*/
static int transaction1_setup_recovery(struct tdb1_context *tdb,
				       tdb1_off_t *magic_offset)
{
	tdb1_len_t recovery_size;
	unsigned char *data, *p;
	const struct tdb1_methods *methods = tdb->transaction->io_methods;
	struct tdb1_record *rec;
	tdb1_off_t recovery_offset, recovery_max_size;
	tdb1_off_t old_map_size = tdb->transaction->old_map_size;
	uint32_t magic, tailer;
	int i;

	/*
	  check that the recovery area has enough space
	*/
	if (tdb1_recovery_allocate(tdb, &recovery_size,
				  &recovery_offset, &recovery_max_size) == -1) {
		return -1;
	}

	data = (unsigned char *)malloc(recovery_size + sizeof(*rec));
	if (data == NULL) {
		tdb->last_error = TDB_ERR_OOM;
		return -1;
	}

	rec = (struct tdb1_record *)data;
	memset(rec, 0, sizeof(*rec));

	rec->magic    = TDB1_RECOVERY_INVALID_MAGIC;
	rec->data_len = recovery_size;
	rec->rec_len  = recovery_max_size;
	rec->key_len  = old_map_size;
	TDB1_CONV(*rec);

	/* build the recovery data into a single blob to allow us to do a single
	   large write, which should be more efficient */
	p = data + sizeof(*rec);
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb1_off_t offset;
		tdb1_len_t length;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * tdb->transaction->block_size;
		length = tdb->transaction->block_size;
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		if (offset >= old_map_size) {
			continue;
		}
		if (offset + length > tdb->transaction->old_map_size) {
			tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						TDB_LOG_ERROR,
						"tdb1_transaction_setup_recovery: transaction data over new region boundary");
			free(data);
			return -1;
		}
		memcpy(p, &offset, 4);
		memcpy(p+4, &length, 4);
		if (TDB1_DOCONV()) {
			tdb1_convert(p, 8);
		}
		/* the recovery area contains the old data, not the
		   new data, so we have to call the original tdb1_read
		   method to get it */
		if (methods->tdb1_read(tdb, offset, p + 8, length, 0) != 0) {
			free(data);
			tdb->last_error = TDB_ERR_IO;
			return -1;
		}
		p += 8 + length;
	}

	/* and the tailer */
	tailer = sizeof(*rec) + recovery_max_size;
	memcpy(p, &tailer, 4);
	if (TDB1_DOCONV()) {
		tdb1_convert(p, 4);
	}

	/* write the recovery data to the recovery area */
	if (methods->tdb1_write(tdb, recovery_offset, data, sizeof(*rec) + recovery_size) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_setup_recovery:"
			   " failed to write recovery data");
		free(data);
		return -1;
	}
	if (transaction1_write_existing(tdb, recovery_offset, data, sizeof(*rec) + recovery_size) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_setup_recovery: failed to write"
			   " secondary recovery data");
		free(data);
		return -1;
	}

	/* as we don't have ordered writes, we have to sync the recovery
	   data before we update the magic to indicate that the recovery
	   data is present */
	if (transaction1_sync(tdb, recovery_offset, sizeof(*rec) + recovery_size) == -1) {
		free(data);
		return -1;
	}

	free(data);

	magic = TDB1_RECOVERY_MAGIC;
	TDB1_CONV(magic);

	*magic_offset = recovery_offset + offsetof(struct tdb1_record, magic);

	if (methods->tdb1_write(tdb, *magic_offset, &magic, sizeof(magic)) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_setup_recovery:"
			   " failed to write recovery magic");
		return -1;
	}
	if (transaction1_write_existing(tdb, *magic_offset, &magic, sizeof(magic)) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_setup_recovery:"
			   " failed to write secondary recovery magic");
		return -1;
	}

	/* ensure the recovery magic marker is on disk */
	if (transaction1_sync(tdb, *magic_offset, sizeof(magic)) == -1) {
		return -1;
	}

	return 0;
}

static int _tdb1_transaction_prepare_commit(struct tdb1_context *tdb)
{
	const struct tdb1_methods *methods;

	if (tdb->transaction == NULL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"tdb1_transaction_prepare_commit:"
					" no transaction");
		return -1;
	}

	if (tdb->transaction->prepared) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"tdb1_transaction_prepare_commit:"
					" transaction already prepared");
		_tdb1_transaction_cancel(tdb);
		return -1;
	}

	if (tdb->transaction->transaction_error) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					"tdb1_transaction_prepare_commit:"
					" transaction error pending");
		_tdb1_transaction_cancel(tdb);
		return -1;
	}


	if (tdb->transaction->nesting != 0) {
		return 0;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		return 0;
	}

	methods = tdb->transaction->io_methods;

	/* if there are any locks pending then the caller has not
	   nested their locks properly, so fail the transaction */
	if (tdb1_have_extra_locks(tdb)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
					"tdb1_transaction_prepare_commit:"
					" locks pending on commit");
		_tdb1_transaction_cancel(tdb);
		return -1;
	}

	/* upgrade the main transaction lock region to a write lock */
	if (tdb1_allrecord_upgrade(tdb) == -1) {
		if (errno != EAGAIN && errno != EINTR) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_prepare_commit:"
				   " failed to upgrade hash locks");
		}
		_tdb1_transaction_cancel(tdb);
		return -1;
	}

	/* get the open lock - this prevents new users attaching to the database
	   during the commit */
	if (tdb1_nest_lock(tdb, TDB1_OPEN_LOCK, F_WRLCK, TDB_LOCK_WAIT) == -1) {
		if (errno != EAGAIN && errno != EINTR) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_prepare_commit:"
				   " failed to get open lock");
		}
		_tdb1_transaction_cancel(tdb);
		return -1;
	}

	if (!(tdb->flags & TDB_NOSYNC)) {
		/* write the recovery data to the end of the file */
		if (transaction1_setup_recovery(tdb, &tdb->transaction->magic_offset) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_prepare_commit:"
				   " failed to setup recovery data");
			_tdb1_transaction_cancel(tdb);
			return -1;
		}
	}

	tdb->transaction->prepared = true;

	/* expand the file to the new size if needed */
	if (tdb->file->map_size != tdb->transaction->old_map_size) {
		if (methods->tdb1_expand_file(tdb, tdb->transaction->old_map_size,
					     tdb->file->map_size -
					     tdb->transaction->old_map_size) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_prepare_commit:"
				   " expansion failed");
			_tdb1_transaction_cancel(tdb);
			return -1;
		}
		tdb->file->map_size = tdb->transaction->old_map_size;
		methods->tdb1_oob(tdb, tdb->file->map_size + 1, 1);
	}

	/* Keep the open lock until the actual commit */

	return 0;
}

/*
   prepare to commit the current transaction
*/
int tdb1_transaction_prepare_commit(struct tdb1_context *tdb)
{
	return _tdb1_transaction_prepare_commit(tdb);
}

/* A repack is worthwhile if the largest is less than half total free. */
static bool repack_worthwhile(struct tdb1_context *tdb)
{
	tdb1_off_t ptr;
	struct tdb1_record rec;
	tdb1_len_t total = 0, largest = 0;

	if (tdb1_ofs_read(tdb, TDB1_FREELIST_TOP, &ptr) == -1) {
		return false;
	}

	while (ptr != 0 && tdb1_rec_free_read(tdb, ptr, &rec) == 0) {
		total += rec.rec_len;
		if (rec.rec_len > largest) {
			largest = rec.rec_len;
		}
		ptr = rec.next;
	}

	return total > largest * 2;
}

/*
  commit the current transaction
*/
int tdb1_transaction_commit(struct tdb1_context *tdb)
{
	const struct tdb1_methods *methods;
	int i;
	bool need_repack = false;

	if (tdb->transaction == NULL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
					"tdb1_transaction_commit:"
					" no transaction");
		return -1;
	}

	if (tdb->transaction->transaction_error) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					"tdb1_transaction_commit:"
					" transaction error pending");
		_tdb1_transaction_cancel(tdb);
		return -1;
	}


	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return 0;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		_tdb1_transaction_cancel(tdb);
		return 0;
	}

	if (!tdb->transaction->prepared) {
		int ret = _tdb1_transaction_prepare_commit(tdb);
		if (ret)
			return ret;
	}

	methods = tdb->transaction->io_methods;

	/* perform all the writes */
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb1_off_t offset;
		tdb1_len_t length;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * tdb->transaction->block_size;
		length = tdb->transaction->block_size;
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		if (methods->tdb1_write(tdb, offset, tdb->transaction->blocks[i], length) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_commit:"
				   " write failed during commit");

			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			tdb->methods = methods;
			tdb1_transaction_recover(tdb);

			_tdb1_transaction_cancel(tdb);

			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_commit: write failed");
			return -1;
		}
		SAFE_FREE(tdb->transaction->blocks[i]);
	}

	/* Do this before we drop lock or blocks. */
	if (tdb->transaction->expanded) {
		need_repack = repack_worthwhile(tdb);
	}

	SAFE_FREE(tdb->transaction->blocks);
	tdb->transaction->num_blocks = 0;

	/* ensure the new data is on disk */
	if (transaction1_sync(tdb, 0, tdb->file->map_size) == -1) {
		return -1;
	}

	/*
	  TODO: maybe write to some dummy hdr field, or write to magic
	  offset without mmap, before the last sync, instead of the
	  utime() call
	*/

	/* on some systems (like Linux 2.6.x) changes via mmap/msync
	   don't change the mtime of the file, this means the file may
	   not be backed up (as tdb rounding to block sizes means that
	   file size changes are quite rare too). The following forces
	   mtime changes when a transaction completes */
#if HAVE_UTIME
	utime(tdb->name, NULL);
#endif

	/* use a transaction cancel to free memory and remove the
	   transaction locks */
	_tdb1_transaction_cancel(tdb);

	if (need_repack) {
		return tdb1_repack(tdb);
	}

	return 0;
}


/*
  recover from an aborted transaction. Must be called with exclusive
  database write access already established (including the open
  lock to prevent new processes attaching)
*/
int tdb1_transaction_recover(struct tdb1_context *tdb)
{
	tdb1_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	uint32_t zero = 0;
	struct tdb1_record rec;

	/* find the recovery area */
	if (tdb1_ofs_read(tdb, TDB1_RECOVERY_HEAD, &recovery_head) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover:"
			   " failed to read recovery head");
		return -1;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return 0;
	}

	/* read the recovery record */
	if (tdb->methods->tdb1_read(tdb, recovery_head, &rec,
				   sizeof(rec), TDB1_DOCONV()) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover:"
			   " failed to read recovery record");
		return -1;
	}

	if (rec.magic != TDB1_RECOVERY_MAGIC) {
		/* there is no valid recovery data */
		return 0;
	}

	if (tdb->read_only) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					"tdb1_transaction_recover:"
					" attempt to recover read only"
					" database");
		return -1;
	}

	recovery_eof = rec.key_len;

	data = (unsigned char *)malloc(rec.data_len);
	if (data == NULL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					"tdb1_transaction_recover:"
					" failed to allocate recovery data");
		return -1;
	}

	/* read the full recovery data */
	if (tdb->methods->tdb1_read(tdb, recovery_head + sizeof(rec), data,
				   rec.data_len, 0) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover:"
			   " failed to read recovery data");
		return -1;
	}

	/* recover the file data */
	p = data;
	while (p+8 < data + rec.data_len) {
		uint32_t ofs, len;
		if (TDB1_DOCONV()) {
			tdb1_convert(p, 8);
		}
		memcpy(&ofs, p, 4);
		memcpy(&len, p+4, 4);

		if (tdb->methods->tdb1_write(tdb, ofs, p+8, len) == -1) {
			free(data);
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_recover: failed to recover"
				   " %d bytes at offset %d", len, ofs);
			return -1;
		}
		p += 8 + len;
	}

	free(data);

	if (transaction1_sync(tdb, 0, tdb->file->map_size) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover: failed to sync recovery");
		return -1;
	}

	/* if the recovery area is after the recovered eof then remove it */
	if (recovery_eof <= recovery_head) {
		if (tdb1_ofs_write(tdb, TDB1_RECOVERY_HEAD, &zero) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_transaction_recover: failed to remove"
				   " recovery head");
			return -1;
		}
	}

	/* remove the recovery magic */
	if (tdb1_ofs_write(tdb, recovery_head + offsetof(struct tdb1_record, magic),
			  &zero) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover: failed to remove"
			   " recovery magic");
		return -1;
	}

	if (transaction1_sync(tdb, 0, recovery_eof) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_transaction_recover:"
			   " failed to sync2 recovery");
		return -1;
	}

	tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
		   "tdb1_transaction_recover: recovered %d byte database",
		   recovery_eof);

	/* all done */
	return 0;
}

/* Any I/O failures we say "needs recovery". */
bool tdb1_needs_recovery(struct tdb1_context *tdb)
{
	tdb1_off_t recovery_head;
	struct tdb1_record rec;

	/* find the recovery area */
	if (tdb1_ofs_read(tdb, TDB1_RECOVERY_HEAD, &recovery_head) == -1) {
		return true;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return false;
	}

	/* read the recovery record */
	if (tdb->methods->tdb1_read(tdb, recovery_head, &rec,
				   sizeof(rec), TDB1_DOCONV()) == -1) {
		return true;
	}

	return (rec.magic == TDB1_RECOVERY_MAGIC);
}
