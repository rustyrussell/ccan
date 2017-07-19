 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              2005
   Copyright (C) Rusty Russell                2010

     ** NOTE! The following LGPL license applies to the ntdb
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
#include <assert.h>
#define SAFE_FREE(ntdb, x) do { if ((x) != NULL) {ntdb->free_fn((void *)x, ntdb->alloc_data); (x)=NULL;} } while(0)

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
    ntdb_free() the old record to place it on the normal ntdb freelist
    before allocating the new record

  - during transactions, keep a linked list of writes all that have
    been performed by intercepting all ntdb_write() calls. The hooked
    transaction versions of ntdb_read() and ntdb_write() check this
    linked list and try to use the elements of the list in preference
    to the real database.

  - don't allow any locks to be held when a transaction starts,
    otherwise we can end up with deadlock (plus lack of lock nesting
    in POSIX locks would mean the lock is lost)

  - if the caller gains a lock during the transaction but doesn't
    release it then fail the commit

  - allow for nested calls to ntdb_transaction_start(), re-using the
    existing transaction record. If the inner transaction is canceled
    then a subsequent commit will fail

  - keep a mirrored copy of the ntdb hash chain heads to allow for the
    fast hash heads scan on traverse, updating the mirrored copy in
    the transaction version of ntdb_write

  - allow callers to mix transaction and non-transaction use of ntdb,
    although once a transaction is started then an exclusive lock is
    gained until the transaction is committed or canceled

  - the commit stategy involves first saving away all modified data
    into a linearised buffer in the transaction recovery area, then
    marking the transaction recovery area with a magic value to
    indicate a valid recovery record. In total 4 fsync/msync calls are
    needed per commit to prevent race conditions. It might be possible
    to reduce this to 3 or even 2 with some more work.

  - check for a valid recovery record on open of the ntdb, while the
    open lock is held. Automatically recover from the transaction
    recovery area if needed, then continue with the open as
    usual. This allows for smooth crash recovery with no administrator
    intervention.

  - if NTDB_NOSYNC is passed to flags in ntdb_open then transactions are
    still available, but fsync/msync calls are made.  This means we
    still are safe against unexpected death during transaction commit,
    but not against machine reboots.
*/

/*
  hold the context of any current transaction
*/
struct ntdb_transaction {
	/* the original io methods - used to do IOs to the real db */
	const struct ntdb_methods *io_methods;

	/* the list of transaction blocks. When a block is first
	   written to, it gets created in this list */
	uint8_t **blocks;
	size_t num_blocks;

	/* non-zero when an internal transaction error has
	   occurred. All write operations will then fail until the
	   transaction is ended */
	int transaction_error;

	/* when inside a transaction we need to keep track of any
	   nested ntdb_transaction_start() calls, as these are allowed,
	   but don't create a new transaction */
	unsigned int nesting;

	/* set when a prepare has already occurred */
	bool prepared;
	ntdb_off_t magic_offset;

	/* old file size before transaction */
	ntdb_len_t old_map_size;
};

/*
  read while in a transaction. We need to check first if the data is in our list
  of transaction elements, then if not do a real read
*/
static enum NTDB_ERROR transaction_read(struct ntdb_context *ntdb, ntdb_off_t off,
				       void *buf, ntdb_len_t len)
{
	size_t blk;
	enum NTDB_ERROR ecode;

	/* break it down into block sized ops */
	while (len + (off % NTDB_PGSIZE) > NTDB_PGSIZE) {
		ntdb_len_t len2 = NTDB_PGSIZE - (off % NTDB_PGSIZE);
		ecode = transaction_read(ntdb, off, buf, len2);
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
		len -= len2;
		off += len2;
		buf = (void *)(len2 + (char *)buf);
	}

	if (len == 0) {
		return NTDB_SUCCESS;
	}

	blk = off / NTDB_PGSIZE;

	/* see if we have it in the block list */
	if (ntdb->transaction->num_blocks <= blk ||
	    ntdb->transaction->blocks[blk] == NULL) {
		/* nope, do a real read */
		ecode = ntdb->transaction->io_methods->tread(ntdb, off, buf, len);
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}
		return 0;
	}

	/* now copy it out of this block */
	memcpy(buf, ntdb->transaction->blocks[blk] + (off % NTDB_PGSIZE), len);
	return NTDB_SUCCESS;

fail:
	ntdb->transaction->transaction_error = 1;
	return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
			  "transaction_read: failed at off=%zu len=%zu",
			  (size_t)off, (size_t)len);
}


/*
  write while in a transaction
*/
static enum NTDB_ERROR transaction_write(struct ntdb_context *ntdb, ntdb_off_t off,
					const void *buf, ntdb_len_t len)
{
	size_t blk;
	enum NTDB_ERROR ecode;

	/* Only a commit is allowed on a prepared transaction */
	if (ntdb->transaction->prepared) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_ERROR,
				   "transaction_write: transaction already"
				   " prepared, write not allowed");
		goto fail;
	}

	/* break it up into block sized chunks */
	while (len + (off % NTDB_PGSIZE) > NTDB_PGSIZE) {
		ntdb_len_t len2 = NTDB_PGSIZE - (off % NTDB_PGSIZE);
		ecode = transaction_write(ntdb, off, buf, len2);
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return NTDB_SUCCESS;
	}

	blk = off / NTDB_PGSIZE;
	off = off % NTDB_PGSIZE;

	if (ntdb->transaction->num_blocks <= blk) {
		uint8_t **new_blocks;
		/* expand the blocks array */
		if (ntdb->transaction->blocks == NULL) {
			new_blocks = (uint8_t **)ntdb->alloc_fn(ntdb,
				    (blk+1)*sizeof(uint8_t *), ntdb->alloc_data);
		} else {
			new_blocks = (uint8_t **)ntdb->expand_fn(
				ntdb->transaction->blocks,
				(blk+1)*sizeof(uint8_t *), ntdb->alloc_data);
		}
		if (new_blocks == NULL) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					   "transaction_write:"
					   " failed to allocate");
			goto fail;
		}
		memset(&new_blocks[ntdb->transaction->num_blocks], 0,
		       (1+(blk - ntdb->transaction->num_blocks))*sizeof(uint8_t *));
		ntdb->transaction->blocks = new_blocks;
		ntdb->transaction->num_blocks = blk+1;
	}

	/* allocate and fill a block? */
	if (ntdb->transaction->blocks[blk] == NULL) {
		ntdb->transaction->blocks[blk] = (uint8_t *)
			ntdb->alloc_fn(ntdb->transaction->blocks, NTDB_PGSIZE,
				   ntdb->alloc_data);
		if (ntdb->transaction->blocks[blk] == NULL) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
					   "transaction_write:"
					   " failed to allocate");
			goto fail;
		}
		memset(ntdb->transaction->blocks[blk], 0, NTDB_PGSIZE);
		if (ntdb->transaction->old_map_size > blk * NTDB_PGSIZE) {
			ntdb_len_t len2 = NTDB_PGSIZE;
			if (len2 + (blk * NTDB_PGSIZE) > ntdb->transaction->old_map_size) {
				len2 = ntdb->transaction->old_map_size - (blk * NTDB_PGSIZE);
			}
			ecode = ntdb->transaction->io_methods->tread(ntdb,
					blk * NTDB_PGSIZE,
					ntdb->transaction->blocks[blk],
					len2);
			if (ecode != NTDB_SUCCESS) {
				ecode = ntdb_logerr(ntdb, ecode,
						   NTDB_LOG_ERROR,
						   "transaction_write:"
						   " failed to"
						   " read old block: %s",
						   strerror(errno));
				SAFE_FREE(ntdb, ntdb->transaction->blocks[blk]);
				goto fail;
			}
		}
	}

	/* overwrite part of an existing block */
	if (buf == NULL) {
		memset(ntdb->transaction->blocks[blk] + off, 0, len);
	} else {
		memcpy(ntdb->transaction->blocks[blk] + off, buf, len);
	}
	return NTDB_SUCCESS;

fail:
	ntdb->transaction->transaction_error = 1;
	return ecode;
}


/*
  write while in a transaction - this variant never expands the transaction blocks, it only
  updates existing blocks. This means it cannot change the recovery size
*/
static void transaction_write_existing(struct ntdb_context *ntdb, ntdb_off_t off,
				       const void *buf, ntdb_len_t len)
{
	size_t blk;

	/* break it up into block sized chunks */
	while (len + (off % NTDB_PGSIZE) > NTDB_PGSIZE) {
		ntdb_len_t len2 = NTDB_PGSIZE - (off % NTDB_PGSIZE);
		transaction_write_existing(ntdb, off, buf, len2);
		len -= len2;
		off += len2;
		if (buf != NULL) {
			buf = (const void *)(len2 + (const char *)buf);
		}
	}

	if (len == 0) {
		return;
	}

	blk = off / NTDB_PGSIZE;
	off = off % NTDB_PGSIZE;

	if (ntdb->transaction->num_blocks <= blk ||
	    ntdb->transaction->blocks[blk] == NULL) {
		return;
	}

	/* overwrite part of an existing block */
	memcpy(ntdb->transaction->blocks[blk] + off, buf, len);
}


/*
  out of bounds check during a transaction
*/
static enum NTDB_ERROR transaction_oob(struct ntdb_context *ntdb,
				      ntdb_off_t off, ntdb_len_t len, bool probe)
{
	if ((off + len >= off && off + len <= ntdb->file->map_size) || probe) {
		return NTDB_SUCCESS;
	}

	ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
		   "ntdb_oob len %lld beyond transaction size %lld",
		   (long long)(off + len),
		   (long long)ntdb->file->map_size);
	return NTDB_ERR_IO;
}

/*
  transaction version of ntdb_expand().
*/
static enum NTDB_ERROR transaction_expand_file(struct ntdb_context *ntdb,
					      ntdb_off_t addition)
{
	enum NTDB_ERROR ecode;

	assert((ntdb->file->map_size + addition) % NTDB_PGSIZE == 0);

	/* add a write to the transaction elements, so subsequent
	   reads see the zero data */
	ecode = transaction_write(ntdb, ntdb->file->map_size, NULL, addition);
	if (ecode == NTDB_SUCCESS) {
		ntdb->file->map_size += addition;
	}
	return ecode;
}

static void *transaction_direct(struct ntdb_context *ntdb, ntdb_off_t off,
				size_t len, bool write_mode)
{
	size_t blk = off / NTDB_PGSIZE, end_blk;

	/* This is wrong for zero-length blocks, but will fail gracefully */
	end_blk = (off + len - 1) / NTDB_PGSIZE;

	/* Can only do direct if in single block and we've already copied. */
	if (write_mode) {
		ntdb->stats.transaction_write_direct++;
		if (blk != end_blk
		    || blk >= ntdb->transaction->num_blocks
		    || ntdb->transaction->blocks[blk] == NULL) {
			ntdb->stats.transaction_write_direct_fail++;
			return NULL;
		}
		return ntdb->transaction->blocks[blk] + off % NTDB_PGSIZE;
	}

	ntdb->stats.transaction_read_direct++;
	/* Single which we have copied? */
	if (blk == end_blk
	    && blk < ntdb->transaction->num_blocks
	    && ntdb->transaction->blocks[blk])
		return ntdb->transaction->blocks[blk] + off % NTDB_PGSIZE;

	/* Otherwise must be all not copied. */
	while (blk <= end_blk) {
		if (blk >= ntdb->transaction->num_blocks)
			break;
		if (ntdb->transaction->blocks[blk]) {
			ntdb->stats.transaction_read_direct_fail++;
			return NULL;
		}
		blk++;
	}
	return ntdb->transaction->io_methods->direct(ntdb, off, len, false);
}

static ntdb_off_t transaction_read_off(struct ntdb_context *ntdb,
				       ntdb_off_t off)
{
	ntdb_off_t ret;
	enum NTDB_ERROR ecode;

	ecode = transaction_read(ntdb, off, &ret, sizeof(ret));
	ntdb_convert(ntdb, &ret, sizeof(ret));
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
	}
	return ret;
}

static enum NTDB_ERROR transaction_write_off(struct ntdb_context *ntdb,
					     ntdb_off_t off, ntdb_off_t val)
{
	ntdb_convert(ntdb, &val, sizeof(val));
	return transaction_write(ntdb, off, &val, sizeof(val));
}

static const struct ntdb_methods transaction_methods = {
	transaction_read,
	transaction_write,
	transaction_oob,
	transaction_expand_file,
	transaction_direct,
	transaction_read_off,
	transaction_write_off,
};

/*
  sync to disk
*/
static enum NTDB_ERROR transaction_sync(struct ntdb_context *ntdb,
				       ntdb_off_t offset, ntdb_len_t length)
{
	if (ntdb->flags & NTDB_NOSYNC) {
		return NTDB_SUCCESS;
	}

	if (fsync(ntdb->file->fd) != 0) {
		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "ntdb_transaction: fsync failed: %s",
				  strerror(errno));
	}
#ifdef MS_SYNC
	if (ntdb->file->map_ptr) {
		ntdb_off_t moffset = offset & ~(getpagesize()-1);
		if (msync(moffset + (char *)ntdb->file->map_ptr,
			  length + (offset - moffset), MS_SYNC) != 0) {
			return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					  "ntdb_transaction: msync failed: %s",
					  strerror(errno));
		}
	}
#endif
	return NTDB_SUCCESS;
}

static void free_transaction_blocks(struct ntdb_context *ntdb)
{
	int i;

	/* free all the transaction blocks */
	for (i=0;i<ntdb->transaction->num_blocks;i++) {
		if (ntdb->transaction->blocks[i] != NULL) {
			ntdb->free_fn(ntdb->transaction->blocks[i],
				      ntdb->alloc_data);
		}
	}
	SAFE_FREE(ntdb, ntdb->transaction->blocks);
	ntdb->transaction->num_blocks = 0;
}

static void _ntdb_transaction_cancel(struct ntdb_context *ntdb)
{
	enum NTDB_ERROR ecode;

	if (ntdb->transaction == NULL) {
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			   "ntdb_transaction_cancel: no transaction");
		return;
	}

	if (ntdb->transaction->nesting != 0) {
		ntdb->transaction->transaction_error = 1;
		ntdb->transaction->nesting--;
		return;
	}

	ntdb->file->map_size = ntdb->transaction->old_map_size;

	free_transaction_blocks(ntdb);

	if (ntdb->transaction->magic_offset) {
		const struct ntdb_methods *methods = ntdb->transaction->io_methods;
		uint64_t invalid = NTDB_RECOVERY_INVALID_MAGIC;

		/* remove the recovery marker */
		ecode = methods->twrite(ntdb, ntdb->transaction->magic_offset,
					&invalid, sizeof(invalid));
		if (ecode == NTDB_SUCCESS)
			ecode = transaction_sync(ntdb,
						 ntdb->transaction->magic_offset,
						 sizeof(invalid));
		if (ecode != NTDB_SUCCESS) {
			ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				   "ntdb_transaction_cancel: failed to remove"
				   " recovery magic");
		}
	}

	if (ntdb->file->allrecord_lock.count)
		ntdb_allrecord_unlock(ntdb, ntdb->file->allrecord_lock.ltype);

	/* restore the normal io methods */
	ntdb->io = ntdb->transaction->io_methods;

	ntdb_transaction_unlock(ntdb, F_WRLCK);

	if (ntdb_has_open_lock(ntdb))
		ntdb_unlock_open(ntdb, F_WRLCK);

	SAFE_FREE(ntdb, ntdb->transaction);
}

/*
  start a ntdb transaction. No token is returned, as only a single
  transaction is allowed to be pending per ntdb_context
*/
_PUBLIC_ enum NTDB_ERROR ntdb_transaction_start(struct ntdb_context *ntdb)
{
	enum NTDB_ERROR ecode;

	ntdb->stats.transactions++;
	/* some sanity checks */
	if (ntdb->flags & NTDB_INTERNAL) {
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				   "ntdb_transaction_start:"
				   " cannot start a transaction on an"
				   " internal ntdb");
	}

	if (ntdb->flags & NTDB_RDONLY) {
		return ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_USE_ERROR,
				   "ntdb_transaction_start:"
				   " cannot start a transaction on a"
				   " read-only ntdb");
	}

	/* cope with nested ntdb_transaction_start() calls */
	if (ntdb->transaction != NULL) {
		if (!(ntdb->flags & NTDB_ALLOW_NESTING)) {
			return ntdb_logerr(ntdb, NTDB_ERR_IO,
					   NTDB_LOG_USE_ERROR,
					   "ntdb_transaction_start:"
					   " already inside transaction");
		}
		ntdb->transaction->nesting++;
		ntdb->stats.transaction_nest++;
		return 0;
	}

	if (ntdb_has_hash_locks(ntdb)) {
		/* the caller must not have any locks when starting a
		   transaction as otherwise we'll be screwed by lack
		   of nested locks in POSIX */
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK,
				   NTDB_LOG_USE_ERROR,
				   "ntdb_transaction_start:"
				   " cannot start a transaction with locks"
				   " held");
	}

	ntdb->transaction = (struct ntdb_transaction *)
		ntdb->alloc_fn(ntdb, sizeof(struct ntdb_transaction),
			       ntdb->alloc_data);
	if (ntdb->transaction == NULL) {
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				   "ntdb_transaction_start:"
				   " cannot allocate");
	}
	memset(ntdb->transaction, 0, sizeof(*ntdb->transaction));

	/* get the transaction write lock. This is a blocking lock. As
	   discussed with Volker, there are a number of ways we could
	   make this async, which we will probably do in the future */
	ecode = ntdb_transaction_lock(ntdb, F_WRLCK);
	if (ecode != NTDB_SUCCESS) {
		SAFE_FREE(ntdb, ntdb->transaction->blocks);
		SAFE_FREE(ntdb, ntdb->transaction);
		return ecode;
	}

	/* get a read lock over entire file. This is upgraded to a write
	   lock during the commit */
	ecode = ntdb_allrecord_lock(ntdb, F_RDLCK, NTDB_LOCK_WAIT, true);
	if (ecode != NTDB_SUCCESS) {
		goto fail_allrecord_lock;
	}

	/* make sure we know about any file expansions already done by
	   anyone else */
	ntdb_oob(ntdb, ntdb->file->map_size, 1, true);
	ntdb->transaction->old_map_size = ntdb->file->map_size;

	/* finally hook the io methods, replacing them with
	   transaction specific methods */
	ntdb->transaction->io_methods = ntdb->io;
	ntdb->io = &transaction_methods;
	return NTDB_SUCCESS;

fail_allrecord_lock:
	ntdb_transaction_unlock(ntdb, F_WRLCK);
	SAFE_FREE(ntdb, ntdb->transaction->blocks);
	SAFE_FREE(ntdb, ntdb->transaction);
	return ecode;
}


/*
  cancel the current transaction
*/
_PUBLIC_ void ntdb_transaction_cancel(struct ntdb_context *ntdb)
{
	ntdb->stats.transaction_cancel++;
	_ntdb_transaction_cancel(ntdb);
}

/*
  work out how much space the linearised recovery data will consume (worst case)
*/
static ntdb_len_t ntdb_recovery_size(struct ntdb_context *ntdb)
{
	ntdb_len_t recovery_size = 0;
	int i;

	recovery_size = 0;
	for (i=0;i<ntdb->transaction->num_blocks;i++) {
		if (i * NTDB_PGSIZE >= ntdb->transaction->old_map_size) {
			break;
		}
		if (ntdb->transaction->blocks[i] == NULL) {
			continue;
		}
		recovery_size += 2*sizeof(ntdb_off_t) + NTDB_PGSIZE;
	}

	return recovery_size;
}

static enum NTDB_ERROR ntdb_recovery_area(struct ntdb_context *ntdb,
					const struct ntdb_methods *methods,
					ntdb_off_t *recovery_offset,
					struct ntdb_recovery_record *rec)
{
	enum NTDB_ERROR ecode;

	*recovery_offset = ntdb_read_off(ntdb,
					offsetof(struct ntdb_header, recovery));
	if (NTDB_OFF_IS_ERR(*recovery_offset)) {
		return NTDB_OFF_TO_ERR(*recovery_offset);
	}

	if (*recovery_offset == 0) {
		rec->max_len = 0;
		return NTDB_SUCCESS;
	}

	ecode = methods->tread(ntdb, *recovery_offset, rec, sizeof(*rec));
	if (ecode != NTDB_SUCCESS)
		return ecode;

	ntdb_convert(ntdb, rec, sizeof(*rec));
	/* ignore invalid recovery regions: can happen in crash */
	if (rec->magic != NTDB_RECOVERY_MAGIC &&
	    rec->magic != NTDB_RECOVERY_INVALID_MAGIC) {
		*recovery_offset = 0;
		rec->max_len = 0;
	}
	return NTDB_SUCCESS;
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

/* Allocates recovery blob, without ntdb_recovery_record at head set up. */
static struct ntdb_recovery_record *alloc_recovery(struct ntdb_context *ntdb,
						  ntdb_len_t *len)
{
	struct ntdb_recovery_record *rec;
	size_t i;
	enum NTDB_ERROR ecode;
	unsigned char *p;
	const struct ntdb_methods *old_methods = ntdb->io;

	rec = ntdb->alloc_fn(ntdb, sizeof(*rec) + ntdb_recovery_size(ntdb),
			 ntdb->alloc_data);
	if (!rec) {
		ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
			   "transaction_setup_recovery:"
			   " cannot allocate");
		return NTDB_ERR_PTR(NTDB_ERR_OOM);
	}

	/* We temporarily revert to the old I/O methods, so we can use
	 * ntdb_access_read */
	ntdb->io = ntdb->transaction->io_methods;

	/* build the recovery data into a single blob to allow us to do a single
	   large write, which should be more efficient */
	p = (unsigned char *)(rec + 1);
	for (i=0;i<ntdb->transaction->num_blocks;i++) {
		ntdb_off_t offset;
		ntdb_len_t length;
		unsigned int off;
		const unsigned char *buffer;

		if (ntdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * NTDB_PGSIZE;
		length = NTDB_PGSIZE;
		if (offset >= ntdb->transaction->old_map_size) {
			continue;
		}

		if (offset + length > ntdb->file->map_size) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
					   "ntdb_transaction_setup_recovery:"
					   " transaction data over new region"
					   " boundary");
			goto fail;
		}
		buffer = ntdb_access_read(ntdb, offset, length, false);
		if (NTDB_PTR_IS_ERR(buffer)) {
			ecode = NTDB_PTR_ERR(buffer);
			goto fail;
		}

		/* Skip over anything the same at the start. */
		off = same(ntdb->transaction->blocks[i], buffer, length);
		offset += off;

		while (off < length) {
			ntdb_len_t len1;
			unsigned int samelen;

			len1 = different(ntdb->transaction->blocks[i] + off,
					buffer + off, length - off,
					sizeof(offset) + sizeof(len1) + 1,
					&samelen);

			memcpy(p, &offset, sizeof(offset));
			memcpy(p + sizeof(offset), &len1, sizeof(len1));
			ntdb_convert(ntdb, p, sizeof(offset) + sizeof(len1));
			p += sizeof(offset) + sizeof(len1);
			memcpy(p, buffer + off, len1);
			p += len1;
			off += len1 + samelen;
			offset += len1 + samelen;
		}
		ntdb_access_release(ntdb, buffer);
	}

	*len = p - (unsigned char *)(rec + 1);
	ntdb->io = old_methods;
	return rec;

fail:
	ntdb->free_fn(rec, ntdb->alloc_data);
	ntdb->io = old_methods;
	return NTDB_ERR_PTR(ecode);
}

static ntdb_off_t create_recovery_area(struct ntdb_context *ntdb,
				      ntdb_len_t rec_length,
				      struct ntdb_recovery_record *rec)
{
	ntdb_off_t off, recovery_off;
	ntdb_len_t addition;
	enum NTDB_ERROR ecode;
	const struct ntdb_methods *methods = ntdb->transaction->io_methods;

	/* round up to a multiple of page size. Overallocate, since each
	 * such allocation forces us to expand the file. */
	rec->max_len = ntdb_expand_adjust(ntdb->file->map_size, rec_length);

	/* Round up to a page. */
	rec->max_len = ((sizeof(*rec) + rec->max_len + NTDB_PGSIZE-1)
			& ~(NTDB_PGSIZE-1))
		- sizeof(*rec);

	off = ntdb->file->map_size;

	/* Restore ->map_size before calling underlying expand_file.
	   Also so that we don't try to expand the file again in the
	   transaction commit, which would destroy the recovery
	   area */
	addition = (ntdb->file->map_size - ntdb->transaction->old_map_size) +
		sizeof(*rec) + rec->max_len;
	ntdb->file->map_size = ntdb->transaction->old_map_size;
	ntdb->stats.transaction_expand_file++;
	ecode = methods->expand_file(ntdb, addition);
	if (ecode != NTDB_SUCCESS) {
		ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
			   "ntdb_recovery_allocate:"
			   " failed to create recovery area");
		return NTDB_ERR_TO_OFF(ecode);
	}

	/* we have to reset the old map size so that we don't try to
	   expand the file again in the transaction commit, which
	   would destroy the recovery area */
	ntdb->transaction->old_map_size = ntdb->file->map_size;

	/* write the recovery header offset and sync - we can sync without a race here
	   as the magic ptr in the recovery record has not been set */
	recovery_off = off;
	ntdb_convert(ntdb, &recovery_off, sizeof(recovery_off));
	ecode = methods->twrite(ntdb, offsetof(struct ntdb_header, recovery),
				&recovery_off, sizeof(ntdb_off_t));
	if (ecode != NTDB_SUCCESS) {
		ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
			   "ntdb_recovery_allocate:"
			   " failed to write recovery head");
		return NTDB_ERR_TO_OFF(ecode);
	}
	transaction_write_existing(ntdb, offsetof(struct ntdb_header, recovery),
				   &recovery_off,
				   sizeof(ntdb_off_t));
	return off;
}

/*
  setup the recovery data that will be used on a crash during commit
*/
static enum NTDB_ERROR transaction_setup_recovery(struct ntdb_context *ntdb)
{
	ntdb_len_t recovery_size = 0;
	ntdb_off_t recovery_off = 0;
	ntdb_off_t old_map_size = ntdb->transaction->old_map_size;
	struct ntdb_recovery_record *recovery;
	const struct ntdb_methods *methods = ntdb->transaction->io_methods;
	uint64_t magic;
	enum NTDB_ERROR ecode;

	recovery = alloc_recovery(ntdb, &recovery_size);
	if (NTDB_PTR_IS_ERR(recovery))
		return NTDB_PTR_ERR(recovery);

	/* If we didn't actually change anything we overwrote? */
	if (recovery_size == 0) {
		/* In theory, we could have just appended data. */
		if (ntdb->transaction->num_blocks * NTDB_PGSIZE
		    < ntdb->transaction->old_map_size) {
			free_transaction_blocks(ntdb);
		}
		ntdb->free_fn(recovery, ntdb->alloc_data);
		return NTDB_SUCCESS;
	}

	ecode = ntdb_recovery_area(ntdb, methods, &recovery_off, recovery);
	if (ecode) {
		ntdb->free_fn(recovery, ntdb->alloc_data);
		return ecode;
	}

	if (recovery->max_len < recovery_size) {
		/* Not large enough. Free up old recovery area. */
		if (recovery_off) {
			ntdb->stats.frees++;
			ecode = add_free_record(ntdb, recovery_off,
						sizeof(*recovery)
						+ recovery->max_len,
						NTDB_LOCK_WAIT, true);
			ntdb->free_fn(recovery, ntdb->alloc_data);
			if (ecode != NTDB_SUCCESS) {
				return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
						  "ntdb_recovery_allocate:"
						  " failed to free previous"
						  " recovery area");
			}

			/* Refresh recovery after add_free_record above. */
			recovery = alloc_recovery(ntdb, &recovery_size);
			if (NTDB_PTR_IS_ERR(recovery))
				return NTDB_PTR_ERR(recovery);
		}

		recovery_off = create_recovery_area(ntdb, recovery_size,
						    recovery);
		if (NTDB_OFF_IS_ERR(recovery_off)) {
			ntdb->free_fn(recovery, ntdb->alloc_data);
			return NTDB_OFF_TO_ERR(recovery_off);
		}
	}

	/* Now we know size, convert rec header. */
	recovery->magic = NTDB_RECOVERY_INVALID_MAGIC;
	recovery->len = recovery_size;
	recovery->eof = old_map_size;
	ntdb_convert(ntdb, recovery, sizeof(*recovery));

	/* write the recovery data to the recovery area */
	ecode = methods->twrite(ntdb, recovery_off, recovery,
				sizeof(*recovery) + recovery_size);
	if (ecode != NTDB_SUCCESS) {
		ntdb->free_fn(recovery, ntdb->alloc_data);
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_setup_recovery:"
				  " failed to write recovery data");
	}
	transaction_write_existing(ntdb, recovery_off, recovery, recovery_size);

	ntdb->free_fn(recovery, ntdb->alloc_data);

	/* as we don't have ordered writes, we have to sync the recovery
	   data before we update the magic to indicate that the recovery
	   data is present */
	ecode = transaction_sync(ntdb, recovery_off, recovery_size);
	if (ecode != NTDB_SUCCESS)
		return ecode;

	magic = NTDB_RECOVERY_MAGIC;
	ntdb_convert(ntdb, &magic, sizeof(magic));

	ntdb->transaction->magic_offset
		= recovery_off + offsetof(struct ntdb_recovery_record, magic);

	ecode = methods->twrite(ntdb, ntdb->transaction->magic_offset,
				&magic, sizeof(magic));
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_setup_recovery:"
				  " failed to write recovery magic");
	}
	transaction_write_existing(ntdb, ntdb->transaction->magic_offset,
				   &magic, sizeof(magic));

	/* ensure the recovery magic marker is on disk */
	return transaction_sync(ntdb, ntdb->transaction->magic_offset,
				sizeof(magic));
}

static enum NTDB_ERROR _ntdb_transaction_prepare_commit(struct ntdb_context *ntdb)
{
	const struct ntdb_methods *methods;
	enum NTDB_ERROR ecode;

	if (ntdb->transaction == NULL) {
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				  "ntdb_transaction_prepare_commit:"
				  " no transaction");
	}

	if (ntdb->transaction->prepared) {
		_ntdb_transaction_cancel(ntdb);
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				  "ntdb_transaction_prepare_commit:"
				  " transaction already prepared");
	}

	if (ntdb->transaction->transaction_error) {
		_ntdb_transaction_cancel(ntdb);
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_ERROR,
				  "ntdb_transaction_prepare_commit:"
				  " transaction error pending");
	}


	if (ntdb->transaction->nesting != 0) {
		return NTDB_SUCCESS;
	}

	/* check for a null transaction */
	if (ntdb->transaction->blocks == NULL) {
		return NTDB_SUCCESS;
	}

	methods = ntdb->transaction->io_methods;

	/* upgrade the main transaction lock region to a write lock */
	ecode = ntdb_allrecord_upgrade(ntdb, NTDB_HASH_LOCK_START);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* get the open lock - this prevents new users attaching to the database
	   during the commit */
	ecode = ntdb_lock_open(ntdb, F_WRLCK, NTDB_LOCK_WAIT|NTDB_LOCK_NOCHECK);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* Sets up ntdb->transaction->recovery and
	 * ntdb->transaction->magic_offset. */
	ecode = transaction_setup_recovery(ntdb);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	ntdb->transaction->prepared = true;

	/* expand the file to the new size if needed */
	if (ntdb->file->map_size != ntdb->transaction->old_map_size) {
		ntdb_len_t add;

		add = ntdb->file->map_size - ntdb->transaction->old_map_size;
		/* Restore original map size for ntdb_expand_file */
		ntdb->file->map_size = ntdb->transaction->old_map_size;
		ecode = methods->expand_file(ntdb, add);
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
	}

	/* Keep the open lock until the actual commit */
	return NTDB_SUCCESS;
}

/*
   prepare to commit the current transaction
*/
_PUBLIC_ enum NTDB_ERROR ntdb_transaction_prepare_commit(struct ntdb_context *ntdb)
{
	return _ntdb_transaction_prepare_commit(ntdb);
}

/*
  commit the current transaction
*/
_PUBLIC_ enum NTDB_ERROR ntdb_transaction_commit(struct ntdb_context *ntdb)
{
	const struct ntdb_methods *methods;
	int i;
	enum NTDB_ERROR ecode;

	if (ntdb->transaction == NULL) {
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				   "ntdb_transaction_commit:"
				   " no transaction");
	}

	ntdb_trace(ntdb, "ntdb_transaction_commit");

	if (ntdb->transaction->nesting != 0) {
		ntdb->transaction->nesting--;
		return NTDB_SUCCESS;
	}

	if (!ntdb->transaction->prepared) {
		ecode = _ntdb_transaction_prepare_commit(ntdb);
		if (ecode != NTDB_SUCCESS) {
			_ntdb_transaction_cancel(ntdb);
			return ecode;
		}
	}

	/* check for a null transaction (prepare_commit may do this!) */
	if (ntdb->transaction->blocks == NULL) {
		_ntdb_transaction_cancel(ntdb);
		return NTDB_SUCCESS;
	}

	methods = ntdb->transaction->io_methods;

	/* perform all the writes */
	for (i=0;i<ntdb->transaction->num_blocks;i++) {
		ntdb_off_t offset;
		ntdb_len_t length;

		if (ntdb->transaction->blocks[i] == NULL) {
			continue;
		}

		offset = i * NTDB_PGSIZE;
		length = NTDB_PGSIZE;

		ecode = methods->twrite(ntdb, offset,
					ntdb->transaction->blocks[i], length);
		if (ecode != NTDB_SUCCESS) {
			/* we've overwritten part of the data and
			   possibly expanded the file, so we need to
			   run the crash recovery code */
			ntdb->io = methods;
			ntdb_transaction_recover(ntdb);

			_ntdb_transaction_cancel(ntdb);

			return ecode;
		}
		SAFE_FREE(ntdb, ntdb->transaction->blocks[i]);
	}

	SAFE_FREE(ntdb, ntdb->transaction->blocks);
	ntdb->transaction->num_blocks = 0;

	/* ensure the new data is on disk */
	ecode = transaction_sync(ntdb, 0, ntdb->file->map_size);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/*
	  TODO: maybe write to some dummy hdr field, or write to magic
	  offset without mmap, before the last sync, instead of the
	  utime() call
	*/

	/* on some systems (like Linux 2.6.x) changes via mmap/msync
	   don't change the mtime of the file, this means the file may
	   not be backed up (as ntdb rounding to block sizes means that
	   file size changes are quite rare too). The following forces
	   mtime changes when a transaction completes */
#if HAVE_UTIME
	utime(ntdb->name, NULL);
#endif

	/* use a transaction cancel to free memory and remove the
	   transaction locks: it "restores" map_size, too. */
	ntdb->transaction->old_map_size = ntdb->file->map_size;
	_ntdb_transaction_cancel(ntdb);

	return NTDB_SUCCESS;
}


/*
  recover from an aborted transaction. Must be called with exclusive
  database write access already established (including the open
  lock to prevent new processes attaching)
*/
enum NTDB_ERROR ntdb_transaction_recover(struct ntdb_context *ntdb)
{
	ntdb_off_t recovery_head, recovery_eof;
	unsigned char *data, *p;
	struct ntdb_recovery_record rec;
	enum NTDB_ERROR ecode;

	/* find the recovery area */
	recovery_head = ntdb_read_off(ntdb, offsetof(struct ntdb_header,recovery));
	if (NTDB_OFF_IS_ERR(recovery_head)) {
		ecode = NTDB_OFF_TO_ERR(recovery_head);
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to read recovery head");
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return NTDB_SUCCESS;
	}

	/* read the recovery record */
	ecode = ntdb_read_convert(ntdb, recovery_head, &rec, sizeof(rec));
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to read recovery record");
	}

	if (rec.magic != NTDB_RECOVERY_MAGIC) {
		/* there is no valid recovery data */
		return NTDB_SUCCESS;
	}

	if (ntdb->flags & NTDB_RDONLY) {
		return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " attempt to recover read only database");
	}

	recovery_eof = rec.eof;

	data = (unsigned char *)ntdb->alloc_fn(ntdb, rec.len, ntdb->alloc_data);
	if (data == NULL) {
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to allocate recovery data");
	}

	/* read the full recovery data */
	ecode = ntdb->io->tread(ntdb, recovery_head + sizeof(rec), data,
				    rec.len);
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to read recovery data");
	}

	/* recover the file data */
	p = data;
	while (p+sizeof(ntdb_off_t)+sizeof(ntdb_len_t) < data + rec.len) {
		ntdb_off_t ofs;
		ntdb_len_t len;
		ntdb_convert(ntdb, p, sizeof(ofs) + sizeof(len));
		memcpy(&ofs, p, sizeof(ofs));
		memcpy(&len, p + sizeof(ofs), sizeof(len));
		p += sizeof(ofs) + sizeof(len);

		ecode = ntdb->io->twrite(ntdb, ofs, p, len);
		if (ecode != NTDB_SUCCESS) {
			ntdb->free_fn(data, ntdb->alloc_data);
			return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
					  "ntdb_transaction_recover:"
					  " failed to recover %zu bytes"
					  " at offset %zu",
					  (size_t)len, (size_t)ofs);
		}
		p += len;
	}

	ntdb->free_fn(data, ntdb->alloc_data);

	ecode = transaction_sync(ntdb, 0, ntdb->file->map_size);
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to sync recovery");
	}

	/* if the recovery area is after the recovered eof then remove it */
	if (recovery_eof <= recovery_head) {
		ecode = ntdb_write_off(ntdb, offsetof(struct ntdb_header,
						    recovery),
				      0);
		if (ecode != NTDB_SUCCESS) {
			return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
					  "ntdb_transaction_recover:"
					  " failed to remove recovery head");
		}
	}

	/* remove the recovery magic */
	ecode = ntdb_write_off(ntdb,
			      recovery_head
			      + offsetof(struct ntdb_recovery_record, magic),
			      NTDB_RECOVERY_INVALID_MAGIC);
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to remove recovery magic");
	}

	ecode = transaction_sync(ntdb, 0, recovery_eof);
	if (ecode != NTDB_SUCCESS) {
		return ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
				  "ntdb_transaction_recover:"
				  " failed to sync2 recovery");
	}

	ntdb_logerr(ntdb, NTDB_SUCCESS, NTDB_LOG_WARNING,
		   "ntdb_transaction_recover: recovered %zu byte database",
		   (size_t)recovery_eof);

	/* all done */
	return NTDB_SUCCESS;
}

ntdb_bool_err ntdb_needs_recovery(struct ntdb_context *ntdb)
{
	ntdb_off_t recovery_head;
	struct ntdb_recovery_record rec;
	enum NTDB_ERROR ecode;

	/* find the recovery area */
	recovery_head = ntdb_read_off(ntdb, offsetof(struct ntdb_header,recovery));
	if (NTDB_OFF_IS_ERR(recovery_head)) {
		return recovery_head;
	}

	if (recovery_head == 0) {
		/* we have never allocated a recovery record */
		return false;
	}

	/* read the recovery record */
	ecode = ntdb_read_convert(ntdb, recovery_head, &rec, sizeof(rec));
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
	}

	return (rec.magic == NTDB_RECOVERY_MAGIC);
}
