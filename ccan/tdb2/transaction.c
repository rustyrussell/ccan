 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              2005
   Copyright (C) Rusty Russell                2010

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

#include "private.h"
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)

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
    tdb_free() the old record to place it on the normal tdb freelist
    before allocating the new record

  - during transactions, keep a linked list of writes all that have
    been performed by intercepting all tdb_write() calls. The hooked
    transaction versions of tdb_read() and tdb_write() check this
    linked list and try to use the elements of the list in preference
    to the real database.

  - don't allow any locks to be held when a transaction starts,
    otherwise we can end up with deadlock (plus lack of lock nesting
    in posix locks would mean the lock is lost)

  - if the caller gains a lock during the transaction but doesn't
    release it then fail the commit

  - allow for nested calls to tdb_transaction_start(), re-using the
    existing transaction record. If the inner transaction is cancelled
    then a subsequent commit will fail

  - keep a mirrored copy of the tdb hash chain heads to allow for the
    fast hash heads scan on traverse, updating the mirrored copy in
    the transaction version of tdb_write

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

  - if TDB_NOSYNC is passed to flags in tdb_open then transactions are
    still available, but no transaction recovery area is used and no
    fsync/msync calls are made.
*/


/*
  hold the context of any current transaction
*/
struct tdb_transaction {
	/* the original io methods - used to do IOs to the real db */
	const struct tdb_methods *io_methods;

	/* the list of transaction blocks. When a block is first
	   written to, it gets created in this list */
	uint8_t **blocks;
	size_t num_blocks;
	size_t last_block_size; /* number of valid bytes in the last block */

	/* non-zero when an internal transaction error has
	   occurred. All write operations will then fail until the
	   transaction is ended */
	int transaction_error;

	/* when inside a transaction we need to keep track of any
	   nested tdb_transaction_start() calls, as these are allowed,
	   but don't create a new transaction */
	int nesting;

	/* set when a prepare has already occurred */
	bool prepared;
	tdb_off_t magic_offset;

	/* old file size before transaction */
	tdb_len_t old_map_size;
};


/*
  read while in a transaction. We need to check first if the data is in our list
  of transaction elements, then if not do a real read
*/
static int transaction_read(struct tdb_context *tdb, tdb_off_t off, void *buf,
			    tdb_len_t len)
{
	size_t blk;

	/* break it down into block sized ops */
	while (len + (off % getpagesize()) > getpagesize()) {
		tdb_len_t len2 = getpagesize() - (off % getpagesize());
		if (transaction_read(tdb, off, buf, len2) != 0) {
			return -1;
		}
		len -= len2;
		off += len2;
		buf = (void *)(len2 + (char *)buf);
	}

	if (len == 0) {
		return 0;
	}

	blk = off / getpagesize();

	/* see if we have it in the block list */
	if (tdb->transaction->num_blocks <= blk ||
	    tdb->transaction->blocks[blk] == NULL) {
		/* nope, do a real read */
		if (tdb->transaction->io_methods->read(tdb, off, buf, len) != 0) {
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
	memcpy(buf, tdb->transaction->blocks[blk] + (off % getpagesize()), len);
	return 0;

fail:
	tdb->ecode = TDB_ERR_IO;
	tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
		 "transaction_read: failed at off=%llu len=%llu\n",
		 (long long)off, (long long)len);
	tdb->transaction->transaction_error = 1;
	return -1;
}


/*
  write while in a transaction
*/
static int transaction_write(struct tdb_context *tdb, tdb_off_t off,
			     const void *buf, tdb_len_t len)
{
	size_t blk;

	/* Only a commit is allowed on a prepared transaction */
	if (tdb->transaction->prepared) {
		tdb->ecode = TDB_ERR_EINVAL;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "transaction_write: transaction already prepared,"
			 " write not allowed\n");
		tdb->transaction->transaction_error = 1;
		return -1;
	}

	/* break it up into block sized chunks */
	while (len + (off % getpagesize()) > getpagesize()) {
		tdb_len_t len2 = getpagesize() - (off % getpagesize());
		if (transaction_write(tdb, off, buf, len2) != 0) {
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

	blk = off / getpagesize();
	off = off % getpagesize();

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
			tdb->ecode = TDB_ERR_OOM;
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
		tdb->transaction->blocks[blk] = (uint8_t *)calloc(getpagesize(), 1);
		if (tdb->transaction->blocks[blk] == NULL) {
			tdb->ecode = TDB_ERR_OOM;
			tdb->transaction->transaction_error = 1;
			return -1;
		}
		if (tdb->transaction->old_map_size > blk * getpagesize()) {
			tdb_len_t len2 = getpagesize();
			if (len2 + (blk * getpagesize()) > tdb->transaction->old_map_size) {
				len2 = tdb->transaction->old_map_size - (blk * getpagesize());
			}
			if (tdb->transaction->io_methods->read(tdb, blk * getpagesize(),
							       tdb->transaction->blocks[blk],
							       len2) != 0) {
				SAFE_FREE(tdb->transaction->blocks[blk]);
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
	tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
		 "transaction_write: failed at off=%llu len=%llu\n",
		 (long long)((blk*getpagesize()) + off),
		 (long long)len);
	tdb->transaction->transaction_error = 1;
	return -1;
}


/*
  write while in a transaction - this varient never expands the transaction blocks, it only
  updates existing blocks. This means it cannot change the recovery size
*/
static void transaction_write_existing(struct tdb_context *tdb, tdb_off_t off,
				       const void *buf, tdb_len_t len)
{
	size_t blk;

	/* break it up into block sized chunks */
	while (len + (off % getpagesize()) > getpagesize()) {
		tdb_len_t len2 = getpagesize() - (off % getpagesize());
		transaction_write_existing(tdb, off, buf, len2);
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return;
	}

	blk = off / getpagesize();
	off = off % getpagesize();

	if (tdb->transaction->num_blocks <= blk ||
	    tdb->transaction->blocks[blk] == NULL) {
		return;
	}

	if (blk == tdb->transaction->num_blocks-1 &&
	    off + len > tdb->transaction->last_block_size) {
		if (off >= tdb->transaction->last_block_size) {
			return;
		}
		len = tdb->transaction->last_block_size - off;
	}

	/* overwrite part of an existing block */
	memcpy(tdb->transaction->blocks[blk] + off, buf, len);
}


/*
  out of bounds check during a transaction
*/
static int transaction_oob(struct tdb_context *tdb, tdb_off_t len, bool probe)
{
	if (len <= tdb->map_size) {
		return 0;
	}
	tdb->ecode = TDB_ERR_IO;
	return -1;
}

/*
  transaction version of tdb_expand().
*/
static int transaction_expand_file(struct tdb_context *tdb, tdb_off_t addition)
{
	/* add a write to the transaction elements, so subsequent
	   reads see the zero data */
	if (transaction_write(tdb, tdb->map_size, NULL, addition) != 0) {
		return -1;
	}
	tdb->map_size += addition;
	return 0;
}

static void *transaction_direct(struct tdb_context *tdb, tdb_off_t off,
				size_t len)
{
	/* FIXME */
	return NULL;
}

static const struct tdb_methods transaction_methods = {
	transaction_read,
	transaction_write,
	transaction_oob,
	transaction_expand_file,
	transaction_direct,
};

/*
  sync to disk
*/
static int transaction_sync(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t length)
{
	if (tdb->flags & TDB_NOSYNC) {
		return 0;
	}

	if (fsync(tdb->fd) != 0) {
		tdb->ecode = TDB_ERR_IO;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction: fsync failed\n");
		return -1;
	}
#ifdef MS_SYNC
	if (tdb->map_ptr) {
		tdb_off_t moffset = offset & ~(getpagesize()-1);
		if (msync(moffset + (char *)tdb->map_ptr,
			  length + (offset - moffset), MS_SYNC) != 0) {
			tdb->ecode = TDB_ERR_IO;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction: msync failed - %s\n",
				 strerror(errno));
			return -1;
		}
	}
#endif
	return 0;
}


static void _tdb_transaction_cancel(struct tdb_context *tdb)
{
	int i;

	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_EINVAL;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_cancel: no transaction\n");
		return;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->transaction_error = 1;
		tdb->transaction->nesting--;
		return;
	}

	tdb->map_size = tdb->transaction->old_map_size;

	/* free all the transaction blocks */
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		if (tdb->transaction->blocks[i] != NULL) {
			free(tdb->transaction->blocks[i]);
		}
	}
	SAFE_FREE(tdb->transaction->blocks);

	if (tdb->transaction->magic_offset) {
		const struct tdb_methods *methods = tdb->transaction->io_methods;
		uint64_t invalid = TDB_RECOVERY_INVALID_MAGIC;

		/* remove the recovery marker */
		if (methods->write(tdb, tdb->transaction->magic_offset,
				   &invalid, sizeof(invalid)) == -1 ||
		    transaction_sync(tdb, tdb->transaction->magic_offset,
				     sizeof(invalid)) == -1) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction_cancel: failed to remove"
				 " recovery magic\n");
		}
	}

	if (tdb->allrecord_lock.count)
		tdb_allrecord_unlock(tdb, tdb->allrecord_lock.ltype);

	/* restore the normal io methods */
	tdb->methods = tdb->transaction->io_methods;

	tdb_transaction_unlock(tdb, F_WRLCK);

	if (tdb_has_open_lock(tdb))
		tdb_unlock_open(tdb);

	SAFE_FREE(tdb->transaction);
}

/*
  start a tdb transaction. No token is returned, as only a single
  transaction is allowed to be pending per tdb_context
*/
int tdb_transaction_start(struct tdb_context *tdb)
{
	/* some sanity checks */
	if (tdb->read_only || (tdb->flags & TDB_INTERNAL)) {
		tdb->ecode = TDB_ERR_EINVAL;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_start: cannot start a transaction"
			 " on a read-only or internal db\n");
		return -1;
	}

	/* cope with nested tdb_transaction_start() calls */
	if (tdb->transaction != NULL) {
		tdb->ecode = TDB_ERR_NESTING;
		return -1;
	}

	if (tdb_has_hash_locks(tdb)) {
		/* the caller must not have any locks when starting a
		   transaction as otherwise we'll be screwed by lack
		   of nested locks in posix */
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_start: cannot start a transaction"
			 " with locks held\n");
		return -1;
	}

	tdb->transaction = (struct tdb_transaction *)
		calloc(sizeof(struct tdb_transaction), 1);
	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	/* get the transaction write lock. This is a blocking lock. As
	   discussed with Volker, there are a number of ways we could
	   make this async, which we will probably do in the future */
	if (tdb_transaction_lock(tdb, F_WRLCK) == -1) {
		SAFE_FREE(tdb->transaction->blocks);
		SAFE_FREE(tdb->transaction);
		return -1;
	}

	/* get a read lock over entire file. This is upgraded to a write
	   lock during the commit */
	if (tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, true) == -1) {
		goto fail_allrecord_lock;
	}

	/* make sure we know about any file expansions already done by
	   anyone else */
	tdb->methods->oob(tdb, tdb->map_size + 1, true);
	tdb->transaction->old_map_size = tdb->map_size;

	/* finally hook the io methods, replacing them with
	   transaction specific methods */
	tdb->transaction->io_methods = tdb->methods;
	tdb->methods = &transaction_methods;
	return 0;

fail_allrecord_lock:
	tdb_transaction_unlock(tdb, F_WRLCK);
	SAFE_FREE(tdb->transaction->blocks);
	SAFE_FREE(tdb->transaction);
	return -1;
}


/*
  cancel the current transaction
*/
void tdb_transaction_cancel(struct tdb_context *tdb)
{
	_tdb_transaction_cancel(tdb);
}

/*
  work out how much space the linearised recovery data will consume
*/
static tdb_len_t tdb_recovery_size(struct tdb_context *tdb)
{
	tdb_len_t recovery_size = 0;
	int i;

	recovery_size = sizeof(tdb_len_t);
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		if (i * getpagesize() >= tdb->transaction->old_map_size) {
			break;
		}
		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}
		recovery_size += 2*sizeof(tdb_off_t);
		if (i == tdb->transaction->num_blocks-1) {
			recovery_size += tdb->transaction->last_block_size;
		} else {
			recovery_size += getpagesize();
		}
	}

	return recovery_size;
}

/*
  allocate the recovery area, or use an existing recovery area if it is
  large enough
*/
static int tdb_recovery_allocate(struct tdb_context *tdb,
				 tdb_len_t *recovery_size,
				 tdb_off_t *recovery_offset,
				 tdb_len_t *recovery_max_size)
{
	struct tdb_recovery_record rec;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	tdb_off_t recovery_head;
	size_t addition;

	recovery_head = tdb_read_off(tdb, offsetof(struct tdb_header,recovery));
	if (recovery_head == TDB_OFF_ERR) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_recovery_allocate:"
			 " failed to read recovery head\n");
		return -1;
	}

	if (recovery_head != 0) {
		if (methods->read(tdb, recovery_head, &rec, sizeof(rec))) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_recovery_allocate:"
				 " failed to read recovery record\n");
			return -1;
		}
		tdb_convert(tdb, &rec, sizeof(rec));
		/* ignore invalid recovery regions: can happen in crash */
		if (rec.magic != TDB_RECOVERY_MAGIC &&
		    rec.magic != TDB_RECOVERY_INVALID_MAGIC) {
			recovery_head = 0;
		}
	}

	*recovery_size = tdb_recovery_size(tdb);

	if (recovery_head != 0 && *recovery_size <= rec.max_len) {
		/* it fits in the existing area */
		*recovery_max_size = rec.max_len;
		*recovery_offset = recovery_head;
		return 0;
	}

	/* we need to free up the old recovery area, then allocate a
	   new one at the end of the file. Note that we cannot use
	   normal allocation to allocate the new one as that might return
	   us an area that is being currently used (as of the start of
	   the transaction) */
	if (recovery_head != 0) {
		if (add_free_record(tdb, recovery_head,
				    sizeof(rec) + rec.max_len) != 0) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_recovery_allocate:"
				 " failed to free previous recovery area\n");
			return -1;
		}
	}

	/* the tdb_free() call might have increased the recovery size */
	*recovery_size = tdb_recovery_size(tdb);

	/* round up to a multiple of page size */
	*recovery_max_size
		= (((sizeof(rec) + *recovery_size) + getpagesize()-1)
		   & ~(getpagesize()-1))
		- sizeof(rec);
	*recovery_offset = tdb->map_size;
	recovery_head = *recovery_offset;

	/* Restore ->map_size before calling underlying expand_file.
	   Also so that we don't try to expand the file again in the
	   transaction commit, which would destroy the recovery
	   area */
	addition = (tdb->map_size - tdb->transaction->old_map_size) +
		sizeof(rec) + *recovery_max_size;
	tdb->map_size = tdb->transaction->old_map_size;
	if (methods->expand_file(tdb, addition) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_recovery_allocate:"
			 " failed to create recovery area\n");
		return -1;
	}

	/* we have to reset the old map size so that we don't try to
	   expand the file again in the transaction commit, which
	   would destroy the recovery area */
	tdb->transaction->old_map_size = tdb->map_size;

	/* write the recovery header offset and sync - we can sync without a race here
	   as the magic ptr in the recovery record has not been set */
	tdb_convert(tdb, &recovery_head, sizeof(recovery_head));
	if (methods->write(tdb, offsetof(struct tdb_header, recovery),
			   &recovery_head, sizeof(tdb_off_t)) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_recovery_allocate:"
			 " failed to write recovery head\n");
		return -1;
	}
	transaction_write_existing(tdb, offsetof(struct tdb_header, recovery),
				   &recovery_head,
				   sizeof(tdb_off_t));
	return 0;
}

/* Set up header for the recovery record. */
static void set_recovery_header(struct tdb_recovery_record *rec,
				uint64_t magic,
				uint64_t datalen, uint64_t actuallen,
				uint64_t oldsize)
{
	rec->magic = magic;
	rec->max_len = actuallen;
	rec->len = datalen;
	rec->eof = oldsize;
}

/*
  setup the recovery data that will be used on a crash during commit
*/
static int transaction_setup_recovery(struct tdb_context *tdb,
				      tdb_off_t *magic_offset)
{
	tdb_len_t recovery_size;
	unsigned char *data, *p;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	struct tdb_recovery_record *rec;
	tdb_off_t recovery_offset, recovery_max_size;
	tdb_off_t old_map_size = tdb->transaction->old_map_size;
	uint64_t magic, tailer;
	int i;

	/*
	  check that the recovery area has enough space
	*/
	if (tdb_recovery_allocate(tdb, &recovery_size,
				  &recovery_offset, &recovery_max_size) == -1) {
		return -1;
	}

	data = (unsigned char *)malloc(recovery_size + sizeof(*rec));
	if (data == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		return -1;
	}

	rec = (struct tdb_recovery_record *)data;
	set_recovery_header(rec, TDB_RECOVERY_INVALID_MAGIC,
			    recovery_size, recovery_max_size, old_map_size);
	tdb_convert(tdb, rec, sizeof(*rec));

	/* build the recovery data into a single blob to allow us to do a single
	   large write, which should be more efficient */
	p = data + sizeof(*rec);
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb_off_t offset;
		tdb_len_t length;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * getpagesize();
		length = getpagesize();
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		if (offset >= old_map_size) {
			continue;
		}
		if (offset + length > tdb->map_size) {
			tdb->ecode = TDB_ERR_CORRUPT;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction_setup_recovery:"
				 " transaction data over new region boundary\n");
			free(data);
			return -1;
		}
		memcpy(p, &offset, sizeof(offset));
		memcpy(p + sizeof(offset), &length, sizeof(length));
		tdb_convert(tdb, p, sizeof(offset) + sizeof(length));

		/* the recovery area contains the old data, not the
		   new data, so we have to call the original tdb_read
		   method to get it */
		if (methods->read(tdb, offset,
				  p + sizeof(offset) + sizeof(length),
				  length) != 0) {
			free(data);
			return -1;
		}
		p += sizeof(offset) + sizeof(length) + length;
	}

	/* and the tailer */
	tailer = sizeof(*rec) + recovery_max_size;
	memcpy(p, &tailer, sizeof(tailer));
	tdb_convert(tdb, p, sizeof(tailer));

	/* write the recovery data to the recovery area */
	if (methods->write(tdb, recovery_offset, data,
			   sizeof(*rec) + recovery_size) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_setup_recovery:"
			 " failed to write recovery data\n");
		free(data);
		return -1;
	}
	transaction_write_existing(tdb, recovery_offset, data,
				   sizeof(*rec) + recovery_size);

	/* as we don't have ordered writes, we have to sync the recovery
	   data before we update the magic to indicate that the recovery
	   data is present */
	if (transaction_sync(tdb, recovery_offset,
			     sizeof(*rec) + recovery_size) == -1) {
		free(data);
		return -1;
	}

	free(data);

	magic = TDB_RECOVERY_MAGIC;
	tdb_convert(tdb, &magic, sizeof(magic));

	*magic_offset = recovery_offset + offsetof(struct tdb_recovery_record,
						   magic);

	if (methods->write(tdb, *magic_offset, &magic, sizeof(magic)) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_setup_recovery:"
			 " failed to write recovery magic\n");
		return -1;
	}
	transaction_write_existing(tdb, *magic_offset, &magic, sizeof(magic));

	/* ensure the recovery magic marker is on disk */
	if (transaction_sync(tdb, *magic_offset, sizeof(magic)) == -1) {
		return -1;
	}

	return 0;
}

static int _tdb_transaction_prepare_commit(struct tdb_context *tdb)
{
	const struct tdb_methods *methods;

	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_EINVAL;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_prepare_commit: no transaction\n");
		return -1;
	}

	if (tdb->transaction->prepared) {
		tdb->ecode = TDB_ERR_EINVAL;
		_tdb_transaction_cancel(tdb);
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_prepare_commit:"
			 " transaction already prepared\n");
		return -1;
	}

	if (tdb->transaction->transaction_error) {
		tdb->ecode = TDB_ERR_IO;
		_tdb_transaction_cancel(tdb);
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_prepare_commit:"
			 " transaction error pending\n");
		return -1;
	}


	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return 0;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		return 0;
	}

	methods = tdb->transaction->io_methods;

	/* upgrade the main transaction lock region to a write lock */
	if (tdb_allrecord_upgrade(tdb) == -1) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_prepare_commit:"
			 " failed to upgrade hash locks\n");
		_tdb_transaction_cancel(tdb);
		return -1;
	}

	/* get the open lock - this prevents new users attaching to the database
	   during the commit */
	if (tdb_lock_open(tdb, TDB_LOCK_WAIT|TDB_LOCK_NOCHECK) == -1) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_prepare_commit:"
			 " failed to get open lock\n");
		_tdb_transaction_cancel(tdb);
		return -1;
	}

	/* Since we have whole db locked, we don't need the expansion lock. */
	if (!(tdb->flags & TDB_NOSYNC)) {
		/* write the recovery data to the end of the file */
		if (transaction_setup_recovery(tdb, &tdb->transaction->magic_offset) == -1) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_transaction_prepare_commit:"
				 " failed to setup recovery data\n");
			_tdb_transaction_cancel(tdb);
			return -1;
		}
	}

	tdb->transaction->prepared = true;

	/* expand the file to the new size if needed */
	if (tdb->map_size != tdb->transaction->old_map_size) {
		tdb_len_t add = tdb->map_size - tdb->transaction->old_map_size;
		/* Restore original map size for tdb_expand_file */
		tdb->map_size = tdb->transaction->old_map_size;
		if (methods->expand_file(tdb, add) == -1) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_transaction_prepare_commit:"
				 " expansion failed\n");
			_tdb_transaction_cancel(tdb);
			return -1;
		}
	}

	/* Keep the open lock until the actual commit */

	return 0;
}

/*
   prepare to commit the current transaction
*/
int tdb_transaction_prepare_commit(struct tdb_context *tdb)
{
	return _tdb_transaction_prepare_commit(tdb);
}

/*
  commit the current transaction
*/
int tdb_transaction_commit(struct tdb_context *tdb)
{
	const struct tdb_methods *methods;
	int i;

	if (tdb->transaction == NULL) {
		tdb->ecode = TDB_ERR_EINVAL;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_commit: no transaction\n");
		return -1;
	}

	tdb_trace(tdb, "tdb_transaction_commit");

	if (tdb->transaction->transaction_error) {
		tdb->ecode = TDB_ERR_IO;
		tdb_transaction_cancel(tdb);
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_transaction_commit: transaction error pending\n");
		return -1;
	}


	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return 0;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		_tdb_transaction_cancel(tdb);
		return 0;
	}

	if (!tdb->transaction->prepared) {
		int ret = _tdb_transaction_prepare_commit(tdb);
		if (ret)
			return ret;
	}

	methods = tdb->transaction->io_methods;

	/* perform all the writes */
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb_off_t offset;
		tdb_len_t length;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * getpagesize();
		length = getpagesize();
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		if (methods->write(tdb, offset, tdb->transaction->blocks[i],
				   length) == -1) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction_commit:"
				 " write failed during commit\n");

			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			tdb->methods = methods;
			tdb_transaction_recover(tdb);

			_tdb_transaction_cancel(tdb);

			return -1;
		}
		SAFE_FREE(tdb->transaction->blocks[i]);
	}

	SAFE_FREE(tdb->transaction->blocks);
	tdb->transaction->num_blocks = 0;

	/* ensure the new data is on disk */
	if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
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
	_tdb_transaction_cancel(tdb);

	return 0;
}


/*
  recover from an aborted transaction. Must be called with exclusive
  database write access already established (including the open
  lock to prevent new processes attaching)
*/
int tdb_transaction_recover(struct tdb_context *tdb)
{
	tdb_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	struct tdb_recovery_record rec;

	/* find the recovery area */
	recovery_head = tdb_read_off(tdb, offsetof(struct tdb_header,recovery));
	if (recovery_head == TDB_OFF_ERR) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " failed to read recovery head\n");
		return -1;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return 0;
	}

	/* read the recovery record */
	if (tdb_read_convert(tdb, recovery_head, &rec, sizeof(rec)) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " failed to read recovery record\n");
		return -1;
	}

	if (rec.magic != TDB_RECOVERY_MAGIC) {
		/* there is no valid recovery data */
		return 0;
	}

	if (tdb->read_only) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " attempt to recover read only database\n");
		tdb->ecode = TDB_ERR_CORRUPT;
		return -1;
	}

	recovery_eof = rec.eof;

	data = (unsigned char *)malloc(rec.len);
	if (data == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " failed to allocate recovery data\n");
		return -1;
	}

	/* read the full recovery data */
	if (tdb->methods->read(tdb, recovery_head + sizeof(rec), data,
			       rec.len) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " failed to read recovery data\n");
		return -1;
	}

	/* recover the file data */
	p = data;
	while (p+sizeof(tdb_off_t)+sizeof(tdb_len_t) < data + rec.len) {
		tdb_off_t ofs;
		tdb_len_t len;
		tdb_convert(tdb, p, sizeof(ofs) + sizeof(len));
		memcpy(&ofs, p, sizeof(ofs));
		memcpy(&len, p + sizeof(ofs), sizeof(len));
		p += sizeof(ofs) + sizeof(len);

		if (tdb->methods->write(tdb, ofs, p, len) == -1) {
			free(data);
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction_recover:"
				 " failed to recover %zu bytes at offset %zu\n",
				 (size_t)len, (size_t)ofs);
			return -1;
		}
		p += len;
	}

	free(data);

	if (transaction_sync(tdb, 0, tdb->map_size) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover: failed to sync recovery\n");
		return -1;
	}

	/* if the recovery area is after the recovered eof then remove it */
	if (recovery_eof <= recovery_head) {
		if (tdb_write_off(tdb, offsetof(struct tdb_header,recovery), 0)
		    == -1) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_transaction_recover:"
				 " failed to remove recovery head\n");
			return -1;
		}
	}

	/* remove the recovery magic */
	if (tdb_write_off(tdb,
			  recovery_head
			  + offsetof(struct tdb_recovery_record, magic),
			  TDB_RECOVERY_INVALID_MAGIC) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover:"
			 " failed to remove recovery magic\n");
		return -1;
	}

	if (transaction_sync(tdb, 0, recovery_eof) == -1) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_transaction_recover: failed to sync2 recovery\n");
		return -1;
	}

	tdb->log(tdb, TDB_DEBUG_TRACE, tdb->log_priv,
		 "tdb_transaction_recover: recovered %zu byte database\n",
		 (size_t)recovery_eof);

	/* all done */
	return 0;
}

/* Any I/O failures we say "needs recovery". */
bool tdb_needs_recovery(struct tdb_context *tdb)
{
	tdb_off_t recovery_head;
	struct tdb_recovery_record rec;

	/* find the recovery area */
	recovery_head = tdb_read_off(tdb, offsetof(struct tdb_header,recovery));
	if (recovery_head == TDB_OFF_ERR) {
		return true;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return false;
	}

	/* read the recovery record */
	if (tdb_read_convert(tdb, recovery_head, &rec, sizeof(rec)) == -1) {
		return true;
	}

	return (rec.magic == TDB_RECOVERY_MAGIC);
}
