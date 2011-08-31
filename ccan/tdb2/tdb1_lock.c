 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003

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

/* list -1 is the alloc list, otherwise a hash chain. */
static tdb1_off_t lock_offset(int list)
{
	return TDB1_FREELIST_TOP + 4*list;
}

/* a byte range locking function - return 0 on success
   this functions locks/unlocks 1 byte at the specified offset.

   On error, errno is also set so that errors are passed back properly
   through tdb1_open().

   note that a len of zero means lock to end of file
*/
int tdb1_brlock(struct tdb_context *tdb,
	       int rw_type, tdb1_off_t offset, size_t len,
	       enum tdb_lock_flags flags)
{
	enum TDB_ERROR ecode = tdb_brlock(tdb, rw_type, offset, len, flags
					  | TDB_LOCK_NOCHECK);
	if (ecode == TDB_SUCCESS)
		return 0;
	tdb->last_error = ecode;
	return -1;
}

int tdb1_brunlock(struct tdb_context *tdb,
		 int rw_type, tdb1_off_t offset, size_t len)
{
	enum TDB_ERROR ecode = tdb_brunlock(tdb, rw_type, offset, len);
	if (ecode == TDB_SUCCESS)
		return 0;
	tdb->last_error = ecode;
	return -1;
}

int tdb1_allrecord_upgrade(struct tdb_context *tdb)
{
	enum TDB_ERROR ecode = tdb_allrecord_upgrade(tdb, TDB1_FREELIST_TOP);
	if (ecode == TDB_SUCCESS)
		return 0;
	tdb->last_error = ecode;
	return -1;
}

static struct tdb_lock *tdb1_find_nestlock(struct tdb_context *tdb,
					   tdb1_off_t offset)
{
	unsigned int i;

	for (i=0; i<tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].off == offset) {
			return &tdb->file->lockrecs[i];
		}
	}
	return NULL;
}

/* lock an offset in the database. */
int tdb1_nest_lock(struct tdb_context *tdb, uint32_t offset, int ltype,
		  enum tdb_lock_flags flags)
{
	enum TDB_ERROR ecode;

	if (offset >= lock_offset(tdb->tdb1.header.hash_size)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_lock: invalid offset %u for"
					" ltype=%d",
					offset, ltype);
		return -1;
	}

	ecode = tdb_nest_lock(tdb, offset, ltype, flags | TDB_LOCK_NOCHECK);
	if (unlikely(ecode != TDB_SUCCESS)) {
		tdb->last_error = ecode;
		return -1;
	}
	return 0;
}

static int tdb1_lock_and_recover(struct tdb_context *tdb)
{
	int ret;

	/* We need to match locking order in transaction commit. */
	if (tdb1_brlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0,
			TDB_LOCK_WAIT|TDB_LOCK_NOCHECK)) {
		return -1;
	}

	if (tdb1_brlock(tdb, F_WRLCK, TDB1_OPEN_LOCK, 1,
			TDB_LOCK_WAIT|TDB_LOCK_NOCHECK)) {
		tdb1_brunlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0);
		return -1;
	}

	ret = tdb1_transaction_recover(tdb);

	tdb1_brunlock(tdb, F_WRLCK, TDB1_OPEN_LOCK, 1);
	tdb1_brunlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0);

	return ret;
}

static bool have_data_locks(const struct tdb_context *tdb)
{
	unsigned int i;

	for (i = 0; i < tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].off >= lock_offset(-1))
			return true;
	}
	return false;
}

static int tdb1_lock_list(struct tdb_context *tdb, int list, int ltype,
			 enum tdb_lock_flags waitflag)
{
	int ret;
	bool check = false;

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->file->allrecord_lock.count &&
	    (ltype == tdb->file->allrecord_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->file->allrecord_lock.count) {
		tdb->last_error = TDB_ERR_LOCK;
		ret = -1;
	} else {
		/* Only check when we grab first data lock. */
		check = !have_data_locks(tdb);
		ret = tdb1_nest_lock(tdb, lock_offset(list), ltype, waitflag);

		if (ret == 0 && check && tdb1_needs_recovery(tdb)) {
			tdb1_nest_unlock(tdb, lock_offset(list), ltype);

			if (tdb1_lock_and_recover(tdb) == -1) {
				return -1;
			}
			return tdb1_lock_list(tdb, list, ltype, waitflag);
		}
	}
	return ret;
}

/* lock a list in the database. list -1 is the alloc list */
int tdb1_lock(struct tdb_context *tdb, int list, int ltype)
{
	int ret;

	ret = tdb1_lock_list(tdb, list, ltype, TDB_LOCK_WAIT);
	/* Don't log for EAGAIN and EINTR: they could have overridden lock fns */
	if (ret && errno != EAGAIN && errno != EINTR) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_lock failed on list %d "
			   "ltype=%d (%s)",  list, ltype, strerror(errno));
	}
	return ret;
}

int tdb1_nest_unlock(struct tdb_context *tdb, uint32_t offset, int ltype)
{
	enum TDB_ERROR ecode;

	/* Sanity checks */
	if (offset >= lock_offset(tdb->tdb1.header.hash_size)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_unlock: offset %u invalid (%d)",
					offset, tdb->tdb1.header.hash_size);
		return -1;
	}

	ecode = tdb_nest_unlock(tdb, offset, ltype);
	if (unlikely(ecode != TDB_SUCCESS)) {
		tdb->last_error = ecode;
		return -1;
	}
	return 0;
}

int tdb1_unlock(struct tdb_context *tdb, int list, int ltype)
{
	/* a global lock allows us to avoid per chain locks */
	if (tdb->file->allrecord_lock.count &&
	    (ltype == tdb->file->allrecord_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->file->allrecord_lock.count) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	return tdb1_nest_unlock(tdb, lock_offset(list), ltype);
}

/*
  get the transaction lock
 */
int tdb1_transaction_lock(struct tdb_context *tdb, int ltype,
			 enum tdb_lock_flags lockflags)
{
	return tdb1_nest_lock(tdb, TDB1_TRANSACTION_LOCK, ltype, lockflags);
}

/*
  release the transaction lock
 */
int tdb1_transaction_unlock(struct tdb_context *tdb, int ltype)
{
	return tdb1_nest_unlock(tdb, TDB1_TRANSACTION_LOCK, ltype);
}

/* lock/unlock entire database.  It can only be upgradable if you have some
 * other way of guaranteeing exclusivity (ie. transaction write lock).
 * We do the locking gradually to avoid being starved by smaller locks. */
int tdb1_allrecord_lock(struct tdb_context *tdb, int ltype,
		       enum tdb_lock_flags flags, bool upgradable)
{
	enum TDB_ERROR ecode;

	/* tdb_lock_gradual() doesn't know about tdb->tdb1.traverse_read. */
	if (tdb->tdb1.traverse_read && !(tdb->flags & TDB_NOLOCK)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK,
					     TDB_LOG_USE_ERROR,
					     "tdb1_allrecord_lock during"
					     " tdb1_read_traverse");
		return -1;
	}

	if (tdb->file->allrecord_lock.count
	    && tdb->file->allrecord_lock.ltype == ltype) {
		tdb->file->allrecord_lock.count++;
		return 0;
	}

	if (tdb1_have_extra_locks(tdb)) {
		/* can't combine global and chain locks */
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK,
					     TDB_LOG_USE_ERROR,
					     "tdb1_allrecord_lock holding"
					     " other locks");
		return -1;
	}

	if (upgradable && ltype != F_RDLCK) {
		/* tdb error: you can't upgrade a write lock! */
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK,
					     TDB_LOG_ERROR,
					     "tdb1_allrecord_lock cannot"
					     " have upgradable write lock");
		return -1;
	}

	/* We cover two kinds of locks:
	 * 1) Normal chain locks.  Taken for almost all operations.
	 * 3) Individual records locks.  Taken after normal or free
	 *    chain locks.
	 *
	 * It is (1) which cause the starvation problem, so we're only
	 * gradual for that. */
	ecode = tdb_lock_gradual(tdb, ltype, flags | TDB_LOCK_NOCHECK,
				 TDB1_FREELIST_TOP, tdb->tdb1.header.hash_size * 4);
	if (ecode != TDB_SUCCESS) {
		tdb->last_error = ecode;
		return -1;
	}

	/* Grab individual record locks. */
	if (tdb1_brlock(tdb, ltype, lock_offset(tdb->tdb1.header.hash_size), 0,
		       flags) == -1) {
		tdb1_brunlock(tdb, ltype, TDB1_FREELIST_TOP,
			     tdb->tdb1.header.hash_size * 4);
		return -1;
	}

	/* FIXME: Temporary cast. */
	tdb->file->allrecord_lock.owner = (void *)(struct tdb1_context *)tdb;
	tdb->file->allrecord_lock.count = 1;
	/* If it's upgradable, it's actually exclusive so we can treat
	 * it as a write lock. */
	tdb->file->allrecord_lock.ltype = upgradable ? F_WRLCK : ltype;
	tdb->file->allrecord_lock.off = upgradable;

	if (tdb1_needs_recovery(tdb)) {
		tdb1_allrecord_unlock(tdb, ltype);
		if (tdb1_lock_and_recover(tdb) == -1) {
			return -1;
		}
		return tdb1_allrecord_lock(tdb, ltype, flags, upgradable);
	}

	return 0;
}



/* unlock entire db */
int tdb1_allrecord_unlock(struct tdb_context *tdb, int ltype)
{
	/* Don't try this during r/o traversal! */
	if (tdb->tdb1.traverse_read) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->file->allrecord_lock.count == 0) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	/* Upgradable locks are marked as write locks. */
	if (tdb->file->allrecord_lock.ltype != ltype
	    && (!tdb->file->allrecord_lock.off || ltype != F_RDLCK)) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->file->allrecord_lock.count > 1) {
		tdb->file->allrecord_lock.count--;
		return 0;
	}

	if (tdb1_brunlock(tdb, ltype, TDB1_FREELIST_TOP, 0)) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_unlockall failed (%s)", strerror(errno));
		return -1;
	}

	tdb->file->allrecord_lock.count = 0;
	tdb->file->allrecord_lock.ltype = 0;

	return 0;
}

/* lock entire database with write lock */
int tdb1_lockall(struct tdb_context *tdb)
{
	return tdb1_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false);
}

/* unlock entire database with write lock */
int tdb1_unlockall(struct tdb_context *tdb)
{
	return tdb1_allrecord_unlock(tdb, F_WRLCK);
}

/* lock entire database with read lock */
int tdb1_lockall_read(struct tdb_context *tdb)
{
	return tdb1_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false);
}

/* unlock entire database with read lock */
int tdb1_unlockall_read(struct tdb_context *tdb)
{
	return tdb1_allrecord_unlock(tdb, F_RDLCK);
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
int tdb1_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	int ret = tdb1_lock(tdb,
			    TDB1_BUCKET(tdb_hash(tdb, key.dptr, key.dsize)),
			    F_WRLCK);
	return ret;
}

int tdb1_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb1_unlock(tdb, TDB1_BUCKET(tdb_hash(tdb, key.dptr, key.dsize)),
			   F_WRLCK);
}

int tdb1_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	int ret;
	ret = tdb1_lock(tdb, TDB1_BUCKET(tdb_hash(tdb, key.dptr, key.dsize)),
			F_RDLCK);
	return ret;
}

int tdb1_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb1_unlock(tdb, TDB1_BUCKET(tdb_hash(tdb, key.dptr, key.dsize)),
			   F_RDLCK);
}

/* record lock stops delete underneath */
int tdb1_lock_record(struct tdb_context *tdb, tdb1_off_t off)
{
	if (tdb->file->allrecord_lock.count) {
		return 0;
	}
	return off ? tdb1_brlock(tdb, F_RDLCK, off, 1, TDB_LOCK_WAIT) : 0;
}

/*
  Write locks override our own fcntl readlocks, so check it here.
  Note this is meant to be F_SETLK, *not* F_SETLKW, as it's not
  an error to fail to get the lock here.
*/
int tdb1_write_lock_record(struct tdb_context *tdb, tdb1_off_t off)
{
	struct tdb1_traverse_lock *i;
	for (i = &tdb->tdb1.travlocks; i; i = i->next)
		if (i->off == off)
			return -1;
	if (tdb->file->allrecord_lock.count) {
		if (tdb->file->allrecord_lock.ltype == F_WRLCK) {
			return 0;
		}
		return -1;
	}
	return tdb1_brlock(tdb, F_WRLCK, off, 1, TDB_LOCK_NOWAIT|TDB_LOCK_PROBE);
}

int tdb1_write_unlock_record(struct tdb_context *tdb, tdb1_off_t off)
{
	if (tdb->file->allrecord_lock.count) {
		return 0;
	}
	return tdb1_brunlock(tdb, F_WRLCK, off, 1);
}

/* fcntl locks don't stack: avoid unlocking someone else's */
int tdb1_unlock_record(struct tdb_context *tdb, tdb1_off_t off)
{
	struct tdb1_traverse_lock *i;
	uint32_t count = 0;

	if (tdb->file->allrecord_lock.count) {
		return 0;
	}

	if (off == 0)
		return 0;
	for (i = &tdb->tdb1.travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb1_brunlock(tdb, F_RDLCK, off, 1) : 0);
}

bool tdb1_have_extra_locks(struct tdb_context *tdb)
{
	unsigned int extra = tdb->file->num_lockrecs;

	/* A transaction holds the lock for all records. */
	if (!tdb->tdb1.transaction && tdb->file->allrecord_lock.count) {
		return true;
	}

	/* We always hold the active lock if CLEAR_IF_FIRST. */
	if (tdb1_find_nestlock(tdb, TDB1_ACTIVE_LOCK)) {
		extra--;
	}

	/* In a transaction, we expect to hold the transaction lock */
	if (tdb->tdb1.transaction
	    && tdb1_find_nestlock(tdb, TDB1_TRANSACTION_LOCK)) {
		extra--;
	}

	return extra;
}

/* The transaction code uses this to remove all locks. */
void tdb1_release_transaction_locks(struct tdb_context *tdb)
{
	unsigned int i, active = 0;

	if (tdb->file->allrecord_lock.count != 0) {
		tdb1_brunlock(tdb, tdb->file->allrecord_lock.ltype, TDB1_FREELIST_TOP, 0);
		tdb->file->allrecord_lock.count = 0;
	}

	for (i=0;i<tdb->file->num_lockrecs;i++) {
		struct tdb_lock *lck = &tdb->file->lockrecs[i];

		/* Don't release the active lock!  Copy it to first entry. */
		if (lck->off == TDB1_ACTIVE_LOCK) {
			tdb->file->lockrecs[active++] = *lck;
		} else {
			tdb1_brunlock(tdb, lck->ltype, lck->off, 1);
		}
	}
	tdb->file->num_lockrecs = active;
	if (tdb->file->num_lockrecs == 0) {
		SAFE_FREE(tdb->file->lockrecs);
	}
}
