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
    in POSIX locks would mean the lock is lost)

  - if the caller gains a lock during the transaction but doesn't
    release it then fail the commit

  - allow for nested calls to tdb_transaction_start(), re-using the
    existing transaction record. If the inner transaction is canceled
    then a subsequent commit will fail

  - keep a mirrored copy of the tdb hash chain heads to allow for the
    fast hash heads scan on traverse, updating the mirrored copy in
    the transaction version of tdb_write

  - allow callers to mix transaction and non-transaction use of tdb,
    although once a transaction is started then an exclusive lock is
    gained until the transaction is committed or canceled

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
	unsigned int nesting;

	/* set when a prepare has already occurred */
	bool prepared;
	tdb_off_t magic_offset;

	/* old file size before transaction */
	tdb_len_t old_map_size;
};

/* This doesn't really need to be pagesize, but we use it for similar reasons. */
#define PAGESIZE 65536

/*
  read while in a transaction. We need to check first if the data is in our list
  of transaction elements, then if not do a real read
*/
static enum TDB_ERROR transaction_read(struct tdb_context *tdb, tdb_off_t off,
				       void *buf, tdb_len_t len)
{
	size_t blk;
	enum TDB_ERROR ecode;

	/* break it down into block sized ops */
	while (len + (off % PAGESIZE) > PAGESIZE) {
		tdb_len_t len2 = PAGESIZE - (off % PAGESIZE);
		ecode = transaction_read(tdb, off, buf, len2);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
		len -= len2;
		off += len2;
		buf = (void *)(len2 + (char *)buf);
	}

	if (len == 0) {
		return TDB_SUCCESS;
	}

	blk = off / PAGESIZE;

	/* see if we have it in the block list */
	if (tdb->transaction->num_blocks <= blk ||
	    tdb->transaction->blocks[blk] == NULL) {
		/* nope, do a real read */
		ecode = tdb->transaction->io_methods->tread(tdb, off, buf, len);
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}
		return 0;
	}

	/* it is in the block list. Now check for the last block */
	if (blk == tdb->transaction->num_blocks-1) {
		if (len > tdb->transaction->last_block_size) {
			ecode = TDB_ERR_IO;
			goto fail;
		}
	}

	/* now copy it out of this block */
	memcpy(buf, tdb->transaction->blocks[blk] + (off % PAGESIZE), len);
	return TDB_SUCCESS;

fail:
	tdb->transaction->transaction_error = 1;
	return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
			  "transaction_read: failed at off=%zu len=%zu",
			  (size_t)off, (size_t)len);
}


/*
  write while in a transaction
*/
static enum TDB_ERROR transaction_write(struct tdb_context *tdb, tdb_off_t off,
					const void *buf, tdb_len_t len)
{
	size_t blk;
	enum TDB_ERROR ecode;

	/* Only a commit is allowed on a prepared transaction */
	if (tdb->transaction->prepared) {
		ecode = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_ERROR,
				   "transaction_write: transaction already"
				   " prepared, write not allowed");
		goto fail;
	}

	/* break it up into block sized chunks */
	while (len + (off % PAGESIZE) > PAGESIZE) {
		tdb_len_t len2 = PAGESIZE - (off % PAGESIZE);
		ecode = transaction_write(tdb, off, buf, len2);
		if (ecode != TDB_SUCCESS) {
			return -1;
		}
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return TDB_SUCCESS;
	}

	blk = off / PAGESIZE;
	off = off % PAGESIZE;

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
			ecode = tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					   "transaction_write:"
					   " failed to allocate");
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
		tdb->transaction->blocks[blk] = (uint8_t *)calloc(PAGESIZE, 1);
		if (tdb->transaction->blocks[blk] == NULL) {
			ecode = tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					   "transaction_write:"
					   " failed to allocate");
			goto fail;
		}
		if (tdb->transaction->old_map_size > blk * PAGESIZE) {
			tdb_len_t len2 = PAGESIZE;
			if (len2 + (blk * PAGESIZE) > tdb->transaction->old_map_size) {
				len2 = tdb->transaction->old_map_size - (blk * PAGESIZE);
			}
			ecode = tdb->transaction->io_methods->tread(tdb,
					blk * PAGESIZE,
					tdb->transaction->blocks[blk],
					len2);
			if (ecode != TDB_SUCCESS) {
				ecode = tdb_logerr(tdb, ecode,
						   TDB_LOG_ERROR,
						   "transaction_write:"
						   " failed to"
						   " read old block: %s",
						   strerror(errno));
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

	return TDB_SUCCESS;

fail:
	tdb->transaction->transaction_error = 1;
	return ecode;
}


/*
  write while in a transaction - this variant never expands the transaction blocks, it only
  updates existing blocks. This means it cannot change the recovery size
*/
static void transaction_write_existing(struct tdb_context *tdb, tdb_off_t off,
				       const void *buf, tdb_len_t len)
{
	size_t blk;

	/* break it up into block sized chunks */
	while (len + (off % PAGESIZE) > PAGESIZE) {
		tdb_len_t len2 = PAGESIZE - (off % PAGESIZE);
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

	blk = off / PAGESIZE;
	off = off % PAGESIZE;

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
static enum TDB_ERROR transaction_oob(struct tdb_context *tdb, tdb_off_t len,
				      bool probe)
{
	if (len <= tdb->file->map_size) {
		return TDB_SUCCESS;
	}
	if (!probe) {
		tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
			   "tdb_oob len %lld beyond transaction size %lld",
			   (long long)len,
			   (long long)tdb->file->map_size);
	}
	return TDB_ERR_IO;
}

/*
  transaction version of tdb_expand().
*/
static enum TDB_ERROR transaction_expand_file(struct tdb_context *tdb,
					      tdb_off_t addition)
{
	enum TDB_ERROR ecode;

	/* add a write to the transaction elements, so subsequent
	   reads see the zero data */
	ecode = transaction_write(tdb, tdb->file->map_size, NULL, addition);
	if (ecode == TDB_SUCCESS) {
		tdb->file->map_size += addition;
	}
	return ecode;
}

static void *transaction_direct(struct tdb_context *tdb, tdb_off_t off,
				size_t len, bool write_mode)
{
	size_t blk = off / PAGESIZE, end_blk;

	/* This is wrong for zero-length blocks, but will fail gracefully */
	end_blk = (off + len - 1) / PAGESIZE;

	/* Can only do direct if in single block and we've already copied. */
	if (write_mode) {
		tdb->stats.transaction_write_direct++;
		if (blk != end_blk
		    || blk >= tdb->transaction->num_blocks
		    || tdb->transaction->blocks[blk] == NULL) {
			tdb->stats.transaction_write_direct_fail++;
			return NULL;
		}
		return tdb->transaction->blocks[blk] + off % PAGESIZE;
	}

	tdb->stats.transaction_read_direct++;
	/* Single which we have copied? */
	if (blk == end_blk
	    && blk < tdb->transaction->num_blocks
	    && tdb->transaction->blocks[blk])
		return tdb->transaction->blocks[blk] + off % PAGESIZE;

	/* Otherwise must be all not copied. */
	while (blk <= end_blk) {
		if (blk >= tdb->transaction->num_blocks)
			break;
		if (tdb->transaction->blocks[blk]) {
			tdb->stats.transaction_read_direct_fail++;
			return NULL;
		}
		blk++;
	}
	return tdb->transaction->io_methods->direct(tdb, off, len, false);
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
static enum TDB_ERROR transaction_sync(struct tdb_context *tdb,
				       tdb_off_t offset, tdb_len_t length)
{
	if (tdb->flags & TDB_NOSYNC) {
		return TDB_SUCCESS;
	}

	if (fsync(tdb->file->fd) != 0) {
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "tdb_transaction: fsync failed: %s",
				  strerror(errno));
	}
#ifdef MS_SYNC
	if (tdb->file->map_ptr) {
		tdb_off_t moffset = offset & ~(getpagesize()-1);
		if (msync(moffset + (char *)tdb->file->map_ptr,
			  length + (offset - moffset), MS_SYNC) != 0) {
			return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					  "tdb_transaction: msync failed: %s",
					  strerror(errno));
		}
	}
#endif
	return TDB_SUCCESS;
}


static void _tdb_transaction_cancel(struct tdb_context *tdb)
{
	int i;
	enum TDB_ERROR ecode;

	if (tdb->transaction == NULL) {
		tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
			   "tdb_transaction_cancel: no transaction");
		return;
	}

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->transaction_error = 1;
		tdb->transaction->nesting--;
		return;
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
		const struct tdb_methods *methods = tdb->transaction->io_methods;
		uint64_t invalid = TDB_RECOVERY_INVALID_MAGIC;

		/* remove the recovery marker */
		ecode = methods->twrite(tdb, tdb->transaction->magic_offset,
					&invalid, sizeof(invalid));
		if (ecode == TDB_SUCCESS)
			ecode = transaction_sync(tdb,
						 tdb->transaction->magic_offset,
						 sizeof(invalid));
		if (ecode != TDB_SUCCESS) {
			tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				   "tdb_transaction_cancel: failed to remove"
				   " recovery magic");
		}
	}

	if (tdb->file->allrecord_lock.count)
		tdb_allrecord_unlock(tdb, tdb->file->allrecord_lock.ltype);

	/* restore the normal io methods */
	tdb->methods = tdb->transaction->io_methods;

	tdb_transaction_unlock(tdb, F_WRLCK);

	if (tdb_has_open_lock(tdb))
		tdb_unlock_open(tdb, F_WRLCK);

	SAFE_FREE(tdb->transaction);
}

/*
  start a tdb transaction. No token is returned, as only a single
  transaction is allowed to be pending per tdb_context
*/
enum TDB_ERROR tdb_transaction_start(struct tdb_context *tdb)
{
	enum TDB_ERROR ecode;

	tdb->stats.transactions++;
	/* some sanity checks */
	if (tdb->read_only || (tdb->flags & TDB_INTERNAL)) {
		return tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
						    TDB_LOG_USE_ERROR,
						    "tdb_transaction_start:"
						    " cannot start a"
						    " transaction on a "
						    "read-only or internal db");
	}

	/* cope with nested tdb_transaction_start() calls */
	if (tdb->transaction != NULL) {
		if (!(tdb->flags & TDB_ALLOW_NESTING)) {
			return tdb->last_error
				= tdb_logerr(tdb, TDB_ERR_IO,
					     TDB_LOG_USE_ERROR,
					     "tdb_transaction_start:"
					     " already inside transaction");
		}
		tdb->transaction->nesting++;
		tdb->stats.transaction_nest++;
		return 0;
	}

	if (tdb_has_hash_locks(tdb)) {
		/* the caller must not have any locks when starting a
		   transaction as otherwise we'll be screwed by lack
		   of nested locks in POSIX */
		return tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK,
						    TDB_LOG_USE_ERROR,
						    "tdb_transaction_start:"
						    " cannot start a"
						    " transaction with locks"
						    " held");
	}

	tdb->transaction = (struct tdb_transaction *)
		calloc(sizeof(struct tdb_transaction), 1);
	if (tdb->transaction == NULL) {
		return tdb->last_error = tdb_logerr(tdb, TDB_ERR_OOM,
						    TDB_LOG_ERROR,
						    "tdb_transaction_start:"
						    " cannot allocate");
	}

	/* get the transaction write lock. This is a blocking lock. As
	   discussed with Volker, there are a number of ways we could
	   make this async, which we will probably do in the future */
	ecode = tdb_transaction_lock(tdb, F_WRLCK);
	if (ecode != TDB_SUCCESS) {
		SAFE_FREE(tdb->transaction->blocks);
		SAFE_FREE(tdb->transaction);
		return tdb->last_error = ecode;
	}

	/* get a read lock over entire file. This is upgraded to a write
	   lock during the commit */
	ecode = tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, true);
	if (ecode != TDB_SUCCESS) {
		goto fail_allrecord_lock;
	}

	/* make sure we know about any file expansions already done by
	   anyone else */
	tdb->methods->oob(tdb, tdb->file->map_size + 1, true);
	tdb->transaction->old_map_size = tdb->file->map_size;

	/* finally hook the io methods, replacing them with
	   transaction specific methods */
	tdb->transaction->io_methods = tdb->methods;
	tdb->methods = &transaction_methods;
	return tdb->last_error = TDB_SUCCESS;

fail_allrecord_lock:
	tdb_transaction_unlock(tdb, F_WRLCK);
	SAFE_FREE(tdb->transaction->blocks);
	SAFE_FREE(tdb->transaction);
	return tdb->last_error = ecode;
}


/*
  cancel the current transaction
*/
void tdb_transaction_cancel(struct tdb_context *tdb)
{
	tdb->stats.transaction_cancel++;
	_tdb_transaction_cancel(tdb);
}

/*
  work out how much space the linearised recovery data will consume (worst case)
*/
static tdb_len_t tdb_recovery_size(struct tdb_context *tdb)
{
	tdb_len_t recovery_size = 0;
	int i;

	recovery_size = 0;
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		if (i * PAGESIZE >= tdb->transaction->old_map_size) {
			break;
		}
		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}
		recovery_size += 2*sizeof(tdb_off_t);
		if (i == tdb->transaction->num_blocks-1) {
			recovery_size += tdb->transaction->last_block_size;
		} else {
			recovery_size += PAGESIZE;
		}
	}

	return recovery_size;
}

static enum TDB_ERROR tdb_recovery_area(struct tdb_context *tdb,
					const struct tdb_methods *methods,
					tdb_off_t *recovery_offset,
					struct tdb_recovery_record *rec)
{
	enum TDB_ERROR ecode;

	*recovery_offset = tdb_read_off(tdb,
					offsetof(struct tdb_header, recovery));
	if (TDB_OFF_IS_ERR(*recovery_offset)) {
		return *recovery_offset;
	}

	if (*recovery_offset == 0) {
		rec->max_len = 0;
		return TDB_SUCCESS;
	}

	ecode = methods->tread(tdb, *recovery_offset, rec, sizeof(*rec));
	if (ecode != TDB_SUCCESS)
		return ecode;

	tdb_convert(tdb, rec, sizeof(*rec));
	/* ignore invalid recovery regions: can happen in crash */
	if (rec->magic != TDB_RECOVERY_MAGIC &&
	    rec->magic != TDB_RECOVERY_INVALID_MAGIC) {
		*recovery_offset = 0;
		rec->max_len = 0;
	}
	return TDB_SUCCESS;
}

static unsigned int same(const unsigned char *new,
			 const unsigned char *old,
			 unsigned int length)
{
	unsigned int i;

	for (i = 0; i < length; i++) {
		if (new[i] != old[i])
			break;
	}
	return i;
}

static unsigned int different(const unsigned char *new,
			      const unsigned char *old,
			      unsigned int length,
			      unsigned int min_same,
			      unsigned int *samelen)
{
	unsigned int i;

	*samelen = 0;
	for (i = 0; i < length; i++) {
		if (new[i] == old[i]) {
			(*samelen)++;
		} else {
			if (*samelen >= min_same) {
				return i - *samelen;
			}
			*samelen = 0;
		}
	}

	if (*samelen < min_same)
		*samelen = 0;
	return length - *samelen;
}

/* Allocates recovery blob, without tdb_recovery_record at head set up. */
static struct tdb_recovery_record *alloc_recovery(struct tdb_context *tdb,
						  tdb_len_t *len)
{
	struct tdb_recovery_record *rec;
	size_t i;
	enum TDB_ERROR ecode;
	unsigned char *p;
	const struct tdb_methods *old_methods = tdb->methods;

	rec = malloc(sizeof(*rec) + tdb_recovery_size(tdb));
	if (!rec) {
		tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
			   "transaction_setup_recovery:"
			   " cannot allocate");
		return TDB_ERR_PTR(TDB_ERR_OOM);
	}

	/* We temporarily revert to the old I/O methods, so we can use
	 * tdb_access_read */
	tdb->methods = tdb->transaction->io_methods;

	/* build the recovery data into a single blob to allow us to do a single
	   large write, which should be more efficient */
	p = (unsigned char *)(rec + 1);
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb_off_t offset;
		tdb_len_t length;
		unsigned int off;
		const unsigned char *buffer;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * PAGESIZE;
		length = PAGESIZE;
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		if (offset >= tdb->transaction->old_map_size) {
			continue;
		}

		if (offset + length > tdb->file->map_size) {
			ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					   "tdb_transaction_setup_recovery:"
					   " transaction data over new region"
					   " boundary");
			goto fail;
		}
		if (offset + length > tdb->transaction->old_map_size) {
			/* Short read at EOF. */
			length = tdb->transaction->old_map_size - offset;
		}
		buffer = tdb_access_read(tdb, offset, length, false);
		if (TDB_PTR_IS_ERR(buffer)) {
			ecode = TDB_PTR_ERR(buffer);
			goto fail;
		}

		/* Skip over anything the same at the start. */
		off = same(tdb->transaction->blocks[i], buffer, length);
		offset += off;

		while (off < length) {
			tdb_len_t len;
			unsigned int samelen;

			len = different(tdb->transaction->blocks[i] + off,
					buffer + off, length - off,
					sizeof(offset) + sizeof(len) + 1,
					&samelen);

			memcpy(p, &offset, sizeof(offset));
			memcpy(p + sizeof(offset), &len, sizeof(len));
			tdb_convert(tdb, p, sizeof(offset) + sizeof(len));
			p += sizeof(offset) + sizeof(len);
			memcpy(p, buffer + off, len);
			p += len;
			off += len + samelen;
			offset += len + samelen;
		}
		tdb_access_release(tdb, buffer);
	}

	*len = p - (unsigned char *)(rec + 1);
	tdb->methods = old_methods;
	return rec;

fail:
	free(rec);
	tdb->methods = old_methods;
	return TDB_ERR_PTR(ecode);
}

static tdb_off_t create_recovery_area(struct tdb_context *tdb,
				      tdb_len_t rec_length,
				      struct tdb_recovery_record *rec)
{
	tdb_off_t off, recovery_off;
	tdb_len_t addition;
	enum TDB_ERROR ecode;
	const struct tdb_methods *methods = tdb->transaction->io_methods;

	/* round up to a multiple of page size. Overallocate, since each
	 * such allocation forces us to expand the file. */
	rec->max_len
		= (((sizeof(*rec) + rec_length + rec_length / 2)
		    + PAGESIZE-1) & ~(PAGESIZE-1))
		- sizeof(*rec);
	off = tdb->file->map_size;

	/* Restore ->map_size before calling underlying expand_file.
	   Also so that we don't try to expand the file again in the
	   transaction commit, which would destroy the recovery
	   area */
	addition = (tdb->file->map_size - tdb->transaction->old_map_size) +
		sizeof(*rec) + rec->max_len;
	tdb->file->map_size = tdb->transaction->old_map_size;
	tdb->stats.transaction_expand_file++;
	ecode = methods->expand_file(tdb, addition);
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_recovery_allocate:"
				  " failed to create recovery area");
	}

	/* we have to reset the old map size so that we don't try to
	   expand the file again in the transaction commit, which
	   would destroy the recovery area */
	tdb->transaction->old_map_size = tdb->file->map_size;

	/* write the recovery header offset and sync - we can sync without a race here
	   as the magic ptr in the recovery record has not been set */
	recovery_off = off;
	tdb_convert(tdb, &recovery_off, sizeof(recovery_off));
	ecode = methods->twrite(tdb, offsetof(struct tdb_header, recovery),
				&recovery_off, sizeof(tdb_off_t));
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_recovery_allocate:"
				  " failed to write recovery head");
	}
	transaction_write_existing(tdb, offsetof(struct tdb_header, recovery),
				   &recovery_off,
				   sizeof(tdb_off_t));
	return off;
}

/*
  setup the recovery data that will be used on a crash during commit
*/
static enum TDB_ERROR transaction_setup_recovery(struct tdb_context *tdb)
{
	tdb_len_t recovery_size = 0;
	tdb_off_t recovery_off = 0;
	tdb_off_t old_map_size = tdb->transaction->old_map_size;
	struct tdb_recovery_record *recovery;
	const struct tdb_methods *methods = tdb->transaction->io_methods;
	uint64_t magic;
	enum TDB_ERROR ecode;

	recovery = alloc_recovery(tdb, &recovery_size);
	if (TDB_PTR_IS_ERR(recovery))
		return TDB_PTR_ERR(recovery);

	ecode = tdb_recovery_area(tdb, methods, &recovery_off, recovery);
	if (ecode) {
		free(recovery);
		return ecode;
	}

	if (recovery->max_len < recovery_size) {
		/* Not large enough. Free up old recovery area. */
		if (recovery_off) {
			tdb->stats.frees++;
			ecode = add_free_record(tdb, recovery_off,
						sizeof(*recovery)
						+ recovery->max_len,
						TDB_LOCK_WAIT, true);
			free(recovery);
			if (ecode != TDB_SUCCESS) {
				return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
						  "tdb_recovery_allocate:"
						  " failed to free previous"
						  " recovery area");
			}

			/* Refresh recovery after add_free_record above. */
			recovery = alloc_recovery(tdb, &recovery_size);
			if (TDB_PTR_IS_ERR(recovery))
				return TDB_PTR_ERR(recovery);
		}

		recovery_off = create_recovery_area(tdb, recovery_size,
						    recovery);
		if (TDB_OFF_IS_ERR(recovery_off)) {
			free(recovery);
			return recovery_off;
		}
	}

	/* Now we know size, convert rec header. */
	recovery->magic = TDB_RECOVERY_INVALID_MAGIC;
	recovery->len = recovery_size;
	recovery->eof = old_map_size;
	tdb_convert(tdb, recovery, sizeof(*recovery));

	/* write the recovery data to the recovery area */
	ecode = methods->twrite(tdb, recovery_off, recovery, recovery_size);
	if (ecode != TDB_SUCCESS) {
		free(recovery);
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_setup_recovery:"
				  " failed to write recovery data");
	}
	transaction_write_existing(tdb, recovery_off, recovery, recovery_size);

	free(recovery);

	/* as we don't have ordered writes, we have to sync the recovery
	   data before we update the magic to indicate that the recovery
	   data is present */
	ecode = transaction_sync(tdb, recovery_off, recovery_size);
	if (ecode != TDB_SUCCESS)
		return ecode;

	magic = TDB_RECOVERY_MAGIC;
	tdb_convert(tdb, &magic, sizeof(magic));

	tdb->transaction->magic_offset
		= recovery_off + offsetof(struct tdb_recovery_record, magic);

	ecode = methods->twrite(tdb, tdb->transaction->magic_offset,
				&magic, sizeof(magic));
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_setup_recovery:"
				  " failed to write recovery magic");
	}
	transaction_write_existing(tdb, tdb->transaction->magic_offset,
				   &magic, sizeof(magic));

	/* ensure the recovery magic marker is on disk */
	return transaction_sync(tdb, tdb->transaction->magic_offset,
				sizeof(magic));
}

static enum TDB_ERROR _tdb_transaction_prepare_commit(struct tdb_context *tdb)
{
	const struct tdb_methods *methods;
	enum TDB_ERROR ecode;

	if (tdb->transaction == NULL) {
		return tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
				  "tdb_transaction_prepare_commit:"
				  " no transaction");
	}

	if (tdb->transaction->prepared) {
		_tdb_transaction_cancel(tdb);
		return tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
				  "tdb_transaction_prepare_commit:"
				  " transaction already prepared");
	}

	if (tdb->transaction->transaction_error) {
		_tdb_transaction_cancel(tdb);
		return tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_ERROR,
				  "tdb_transaction_prepare_commit:"
				  " transaction error pending");
	}


	if (tdb->transaction->nesting != 0) {
		return TDB_SUCCESS;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		return TDB_SUCCESS;
	}

	methods = tdb->transaction->io_methods;

	/* upgrade the main transaction lock region to a write lock */
	ecode = tdb_allrecord_upgrade(tdb);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* get the open lock - this prevents new users attaching to the database
	   during the commit */
	ecode = tdb_lock_open(tdb, F_WRLCK, TDB_LOCK_WAIT|TDB_LOCK_NOCHECK);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* Since we have whole db locked, we don't need the expansion lock. */
	if (!(tdb->flags & TDB_NOSYNC)) {
		/* Sets up tdb->transaction->recovery and
		 * tdb->transaction->magic_offset. */
		ecode = transaction_setup_recovery(tdb);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
	}

	tdb->transaction->prepared = true;

	/* expand the file to the new size if needed */
	if (tdb->file->map_size != tdb->transaction->old_map_size) {
		tdb_len_t add;

		add = tdb->file->map_size - tdb->transaction->old_map_size;
		/* Restore original map size for tdb_expand_file */
		tdb->file->map_size = tdb->transaction->old_map_size;
		ecode = methods->expand_file(tdb, add);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
	}

	/* Keep the open lock until the actual commit */
	return TDB_SUCCESS;
}

/*
   prepare to commit the current transaction
*/
enum TDB_ERROR tdb_transaction_prepare_commit(struct tdb_context *tdb)
{
	return _tdb_transaction_prepare_commit(tdb);
}

/*
  commit the current transaction
*/
enum TDB_ERROR tdb_transaction_commit(struct tdb_context *tdb)
{
	const struct tdb_methods *methods;
	int i;
	enum TDB_ERROR ecode;

	if (tdb->transaction == NULL) {
		return tdb->last_error = tdb_logerr(tdb, TDB_ERR_EINVAL,
						    TDB_LOG_USE_ERROR,
						    "tdb_transaction_commit:"
						    " no transaction");
	}

	tdb_trace(tdb, "tdb_transaction_commit");

	if (tdb->transaction->nesting != 0) {
		tdb->transaction->nesting--;
		return tdb->last_error = TDB_SUCCESS;
	}

	/* check for a null transaction */
	if (tdb->transaction->blocks == NULL) {
		_tdb_transaction_cancel(tdb);
		return tdb->last_error = TDB_SUCCESS;
	}

	if (!tdb->transaction->prepared) {
		ecode = _tdb_transaction_prepare_commit(tdb);
		if (ecode != TDB_SUCCESS) {
			_tdb_transaction_cancel(tdb);
			return tdb->last_error = ecode;
		}
	}

	methods = tdb->transaction->io_methods;

	/* perform all the writes */
	for (i=0;i<tdb->transaction->num_blocks;i++) {
		tdb_off_t offset;
		tdb_len_t length;

		if (tdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * PAGESIZE;
		length = PAGESIZE;
		if (i == tdb->transaction->num_blocks-1) {
			length = tdb->transaction->last_block_size;
		}

		ecode = methods->twrite(tdb, offset,
					tdb->transaction->blocks[i], length);
		if (ecode != TDB_SUCCESS) {
			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			tdb->methods = methods;
			tdb_transaction_recover(tdb);

			_tdb_transaction_cancel(tdb);

			return tdb->last_error = ecode;
		}
		SAFE_FREE(tdb->transaction->blocks[i]);
	}

	SAFE_FREE(tdb->transaction->blocks);
	tdb->transaction->num_blocks = 0;

	/* ensure the new data is on disk */
	ecode = transaction_sync(tdb, 0, tdb->file->map_size);
	if (ecode != TDB_SUCCESS) {
		return tdb->last_error = ecode;
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
	   transaction locks: it "restores" map_size, too. */
	tdb->transaction->old_map_size = tdb->file->map_size;
	_tdb_transaction_cancel(tdb);

	return tdb->last_error = TDB_SUCCESS;
}


/*
  recover from an aborted transaction. Must be called with exclusive
  database write access already established (including the open
  lock to prevent new processes attaching)
*/
enum TDB_ERROR tdb_transaction_recover(struct tdb_context *tdb)
{
	tdb_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	struct tdb_recovery_record rec;
	enum TDB_ERROR ecode;

	/* find the recovery area */
	recovery_head = tdb_read_off(tdb, offsetof(struct tdb_header,recovery));
	if (TDB_OFF_IS_ERR(recovery_head)) {
		return tdb_logerr(tdb, recovery_head, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to read recovery head");
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return TDB_SUCCESS;
	}

	/* read the recovery record */
	ecode = tdb_read_convert(tdb, recovery_head, &rec, sizeof(rec));
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to read recovery record");
	}

	if (rec.magic != TDB_RECOVERY_MAGIC) {
		/* there is no valid recovery data */
		return TDB_SUCCESS;
	}

	if (tdb->read_only) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " attempt to recover read only database");
	}

	recovery_eof = rec.eof;

	data = (unsigned char *)malloc(rec.len);
	if (data == NULL) {
		return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to allocate recovery data");
	}

	/* read the full recovery data */
	ecode = tdb->methods->tread(tdb, recovery_head + sizeof(rec), data,
				    rec.len);
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to read recovery data");
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

		ecode = tdb->methods->twrite(tdb, ofs, p, len);
		if (ecode != TDB_SUCCESS) {
			free(data);
			return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
					  "tdb_transaction_recover:"
					  " failed to recover %zu bytes"
					  " at offset %zu",
					  (size_t)len, (size_t)ofs);
		}
		p += len;
	}

	free(data);

	ecode = transaction_sync(tdb, 0, tdb->file->map_size);
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to sync recovery");
	}

	/* if the recovery area is after the recovered eof then remove it */
	if (recovery_eof <= recovery_head) {
		ecode = tdb_write_off(tdb, offsetof(struct tdb_header,
						    recovery),
				      0);
		if (ecode != TDB_SUCCESS) {
			return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
					  "tdb_transaction_recover:"
					  " failed to remove recovery head");
		}
	}

	/* remove the recovery magic */
	ecode = tdb_write_off(tdb,
			      recovery_head
			      + offsetof(struct tdb_recovery_record, magic),
			      TDB_RECOVERY_INVALID_MAGIC);
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to remove recovery magic");
	}

	ecode = transaction_sync(tdb, 0, recovery_eof);
	if (ecode != TDB_SUCCESS) {
		return tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				  "tdb_transaction_recover:"
				  " failed to sync2 recovery");
	}

	tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
		   "tdb_transaction_recover: recovered %zu byte database",
		   (size_t)recovery_eof);

	/* all done */
	return TDB_SUCCESS;
}

tdb_bool_err tdb_needs_recovery(struct tdb_context *tdb)
{
	tdb_off_t recovery_head;
	struct tdb_recovery_record rec;
	enum TDB_ERROR ecode;

	/* find the recovery area */
	recovery_head = tdb_read_off(tdb, offsetof(struct tdb_header,recovery));
	if (TDB_OFF_IS_ERR(recovery_head)) {
		return recovery_head;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return false;
	}

	/* read the recovery record */
	ecode = tdb_read_convert(tdb, recovery_head, &rec, sizeof(rec));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	return (rec.magic == TDB_RECOVERY_MAGIC);
}
