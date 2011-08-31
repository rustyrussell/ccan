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

static int fcntl_lock(struct tdb1_context *tdb,
		      int rw, off_t off, off_t len, bool waitflag)
{
	struct flock fl;

	fl.l_type = rw;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = len;
	fl.l_pid = 0;

	if (waitflag)
		return fcntl(tdb->fd, F_SETLKW, &fl);
	else
		return fcntl(tdb->fd, F_SETLK, &fl);
}

static int fcntl_unlock(struct tdb1_context *tdb, int rw, off_t off, off_t len)
{
	struct flock fl;
#if 0 /* Check they matched up locks and unlocks correctly. */
	char line[80];
	FILE *locks;
	bool found = false;

	locks = fopen("/proc/locks", "r");

	while (fgets(line, 80, locks)) {
		char *p;
		int type, start, l;

		/* eg. 1: FLOCK  ADVISORY  WRITE 2440 08:01:2180826 0 EOF */
		p = strchr(line, ':') + 1;
		if (strncmp(p, " POSIX  ADVISORY  ", strlen(" POSIX  ADVISORY  ")))
			continue;
		p += strlen(" FLOCK  ADVISORY  ");
		if (strncmp(p, "READ  ", strlen("READ  ")) == 0)
			type = F_RDLCK;
		else if (strncmp(p, "WRITE ", strlen("WRITE ")) == 0)
			type = F_WRLCK;
		else
			abort();
		p += 6;
		if (atoi(p) != getpid())
			continue;
		p = strchr(strchr(p, ' ') + 1, ' ') + 1;
		start = atoi(p);
		p = strchr(p, ' ') + 1;
		if (strncmp(p, "EOF", 3) == 0)
			l = 0;
		else
			l = atoi(p) - start + 1;

		if (off == start) {
			if (len != l) {
				fprintf(stderr, "Len %u should be %u: %s",
					(int)len, l, line);
				abort();
			}
			if (type != rw) {
				fprintf(stderr, "Type %s wrong: %s",
					rw == F_RDLCK ? "READ" : "WRITE", line);
				abort();
			}
			found = true;
			break;
		}
	}

	if (!found) {
		fprintf(stderr, "Unlock on %u@%u not found!\n",
			(int)off, (int)len);
		abort();
	}

	fclose(locks);
#endif

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = len;
	fl.l_pid = 0;

	return fcntl(tdb->fd, F_SETLKW, &fl);
}

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
int tdb1_brlock(struct tdb1_context *tdb,
	       int rw_type, tdb1_off_t offset, size_t len,
	       enum tdb_lock_flags flags)
{
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	if ((rw_type == F_WRLCK) && (tdb->read_only || tdb->traverse_read)) {
		tdb->last_error = TDB_ERR_RDONLY;
		return -1;
	}

	do {
		ret = fcntl_lock(tdb, rw_type, offset, len,
				 flags & TDB_LOCK_WAIT);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		tdb->last_error = TDB_ERR_LOCK;
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!(flags & TDB_LOCK_PROBE) && errno != EAGAIN) {
			tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				   "tdb1_brlock failed (fd=%d) at offset %d rw_type=%d flags=%d len=%d",
				   tdb->fd, offset, rw_type, flags, (int)len);
		}
		return -1;
	}
	return 0;
}

int tdb1_brunlock(struct tdb1_context *tdb,
		 int rw_type, tdb1_off_t offset, size_t len)
{
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	do {
		ret = fcntl_unlock(tdb, rw_type, offset, len);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_brunlock failed (fd=%d) at offset"
					" %d rw_type=%d len=%d",
					tdb->fd, offset, rw_type, (int)len);
	}
	return ret;
}

/*
  upgrade a read lock to a write lock. This needs to be handled in a
  special way as some OSes (such as solaris) have too conservative
  deadlock detection and claim a deadlock when progress can be
  made. For those OSes we may loop for a while.
*/
int tdb1_allrecord_upgrade(struct tdb1_context *tdb)
{
	int count = 1000;

	if (tdb->allrecord_lock.count != 1) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_allrecord_upgrade failed: "
					"count %u too high",
					tdb->allrecord_lock.count);
		return -1;
	}

	if (tdb->allrecord_lock.off != 1) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_allrecord_upgrade failed:"
					" already upgraded?");
		return -1;
	}

	while (count--) {
		struct timeval tv;
		if (tdb1_brlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0,
			       TDB_LOCK_WAIT|TDB_LOCK_PROBE) == 0) {
			tdb->allrecord_lock.ltype = F_WRLCK;
			tdb->allrecord_lock.off = 0;
			return 0;
		}
		if (errno != EDEADLK) {
			break;
		}
		/* sleep for as short a time as we can - more portable than usleep() */
		tv.tv_sec = 0;
		tv.tv_usec = 1;
		select(0, NULL, NULL, NULL, &tv);
	}
	tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				"tdb1_allrecord_upgrade failed");
	return -1;
}

static struct tdb1_lock_type *tdb1_find_nestlock(struct tdb1_context *tdb,
						 tdb1_off_t offset)
{
	unsigned int i;

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].off == offset) {
			return &tdb->lockrecs[i];
		}
	}
	return NULL;
}

/* lock an offset in the database. */
int tdb1_nest_lock(struct tdb1_context *tdb, uint32_t offset, int ltype,
		  enum tdb_lock_flags flags)
{
	struct tdb1_lock_type *new_lck;

	if (offset >= lock_offset(tdb->header.hash_size)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_lock: invalid offset %u for"
					" ltype=%d",
					offset, ltype);
		return -1;
	}
	if (tdb->flags & TDB_NOLOCK)
		return 0;

	new_lck = tdb1_find_nestlock(tdb, offset);
	if (new_lck) {
		/*
		 * Just increment the in-memory struct, posix locks
		 * don't stack.
		 */
		new_lck->count++;
		return 0;
	}

	new_lck = (struct tdb1_lock_type *)realloc(
		tdb->lockrecs,
		sizeof(*tdb->lockrecs) * (tdb->num_lockrecs+1));
	if (new_lck == NULL) {
		errno = ENOMEM;
		return -1;
	}
	tdb->lockrecs = new_lck;

	/* Since fcntl locks don't nest, we do a lock for the first one,
	   and simply bump the count for future ones */
	if (tdb1_brlock(tdb, ltype, offset, 1, flags)) {
		return -1;
	}

	tdb->lockrecs[tdb->num_lockrecs].off = offset;
	tdb->lockrecs[tdb->num_lockrecs].count = 1;
	tdb->lockrecs[tdb->num_lockrecs].ltype = ltype;
	tdb->num_lockrecs++;

	return 0;
}

static int tdb1_lock_and_recover(struct tdb1_context *tdb)
{
	int ret;

	/* We need to match locking order in transaction commit. */
	if (tdb1_brlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0, TDB_LOCK_WAIT)) {
		return -1;
	}

	if (tdb1_brlock(tdb, F_WRLCK, TDB1_OPEN_LOCK, 1, TDB_LOCK_WAIT)) {
		tdb1_brunlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0);
		return -1;
	}

	ret = tdb1_transaction_recover(tdb);

	tdb1_brunlock(tdb, F_WRLCK, TDB1_OPEN_LOCK, 1);
	tdb1_brunlock(tdb, F_WRLCK, TDB1_FREELIST_TOP, 0);

	return ret;
}

static bool have_data_locks(const struct tdb1_context *tdb)
{
	unsigned int i;

	for (i = 0; i < tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].off >= lock_offset(-1))
			return true;
	}
	return false;
}

static int tdb1_lock_list(struct tdb1_context *tdb, int list, int ltype,
			 enum tdb_lock_flags waitflag)
{
	int ret;
	bool check = false;

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->allrecord_lock.count &&
	    (ltype == tdb->allrecord_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->allrecord_lock.count) {
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
int tdb1_lock(struct tdb1_context *tdb, int list, int ltype)
{
	int ret;

	ret = tdb1_lock_list(tdb, list, ltype, TDB_LOCK_WAIT);
	if (ret) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_lock failed on list %d "
			   "ltype=%d (%s)",  list, ltype, strerror(errno));
	}
	return ret;
}

int tdb1_nest_unlock(struct tdb1_context *tdb, uint32_t offset, int ltype)
{
	int ret = -1;
	struct tdb1_lock_type *lck;

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	/* Sanity checks */
	if (offset >= lock_offset(tdb->header.hash_size)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_unlock: offset %u invalid (%d)",
					offset, tdb->header.hash_size);
		return ret;
	}

	lck = tdb1_find_nestlock(tdb, offset);
	if ((lck == NULL) || (lck->count == 0)) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					"tdb1_unlock: count is 0");
		return -1;
	}

	if (lck->count > 1) {
		lck->count--;
		return 0;
	}

	/*
	 * This lock has count==1 left, so we need to unlock it in the
	 * kernel. We don't bother with decrementing the in-memory array
	 * element, we're about to overwrite it with the last array element
	 * anyway.
	 */

	ret = tdb1_brunlock(tdb, ltype, offset, 1);

	/*
	 * Shrink the array by overwriting the element just unlocked with the
	 * last array element.
	 */
	*lck = tdb->lockrecs[--tdb->num_lockrecs];

	/*
	 * We don't bother with realloc when the array shrinks, but if we have
	 * a completely idle tdb we should get rid of the locked array.
	 */

	if (tdb->num_lockrecs == 0) {
		SAFE_FREE(tdb->lockrecs);
	}

	return ret;
}

int tdb1_unlock(struct tdb1_context *tdb, int list, int ltype)
{
	/* a global lock allows us to avoid per chain locks */
	if (tdb->allrecord_lock.count &&
	    (ltype == tdb->allrecord_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->allrecord_lock.count) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	return tdb1_nest_unlock(tdb, lock_offset(list), ltype);
}

/*
  get the transaction lock
 */
int tdb1_transaction_lock(struct tdb1_context *tdb, int ltype,
			 enum tdb_lock_flags lockflags)
{
	return tdb1_nest_lock(tdb, TDB1_TRANSACTION_LOCK, ltype, lockflags);
}

/*
  release the transaction lock
 */
int tdb1_transaction_unlock(struct tdb1_context *tdb, int ltype)
{
	return tdb1_nest_unlock(tdb, TDB1_TRANSACTION_LOCK, ltype);
}

/* Returns 0 if all done, -1 if error, 1 if ok. */
static int tdb1_allrecord_check(struct tdb1_context *tdb, int ltype,
			       enum tdb_lock_flags flags, bool upgradable)
{
	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->allrecord_lock.count && tdb->allrecord_lock.ltype == ltype) {
		tdb->allrecord_lock.count++;
		return 0;
	}

	if (tdb->allrecord_lock.count) {
		/* a global lock of a different type exists */
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb1_have_extra_locks(tdb)) {
		/* can't combine global and chain locks */
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (upgradable && ltype != F_RDLCK) {
		/* tdb error: you can't upgrade a write lock! */
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}
	return 1;
}

/* We only need to lock individual bytes, but Linux merges consecutive locks
 * so we lock in contiguous ranges. */
static int tdb1_chainlock_gradual(struct tdb1_context *tdb,
				 int ltype, enum tdb_lock_flags flags,
				 size_t off, size_t len)
{
	int ret;
	enum tdb_lock_flags nb_flags = (flags & ~TDB_LOCK_WAIT);

	if (len <= 4) {
		/* Single record.  Just do blocking lock. */
		return tdb1_brlock(tdb, ltype, off, len, flags);
	}

	/* First we try non-blocking. */
	ret = tdb1_brlock(tdb, ltype, off, len, nb_flags);
	if (ret == 0) {
		return 0;
	}

	/* Try locking first half, then second. */
	ret = tdb1_chainlock_gradual(tdb, ltype, flags, off, len / 2);
	if (ret == -1)
		return -1;

	ret = tdb1_chainlock_gradual(tdb, ltype, flags,
				    off + len / 2, len - len / 2);
	if (ret == -1) {
		tdb1_brunlock(tdb, ltype, off, len / 2);
		return -1;
	}
	return 0;
}

/* lock/unlock entire database.  It can only be upgradable if you have some
 * other way of guaranteeing exclusivity (ie. transaction write lock).
 * We do the locking gradually to avoid being starved by smaller locks. */
int tdb1_allrecord_lock(struct tdb1_context *tdb, int ltype,
		       enum tdb_lock_flags flags, bool upgradable)
{
	switch (tdb1_allrecord_check(tdb, ltype, flags, upgradable)) {
	case -1:
		return -1;
	case 0:
		return 0;
	}

	/* We cover two kinds of locks:
	 * 1) Normal chain locks.  Taken for almost all operations.
	 * 3) Individual records locks.  Taken after normal or free
	 *    chain locks.
	 *
	 * It is (1) which cause the starvation problem, so we're only
	 * gradual for that. */
	if (tdb1_chainlock_gradual(tdb, ltype, flags, TDB1_FREELIST_TOP,
				  tdb->header.hash_size * 4) == -1) {
		return -1;
	}

	/* Grab individual record locks. */
	if (tdb1_brlock(tdb, ltype, lock_offset(tdb->header.hash_size), 0,
		       flags) == -1) {
		tdb1_brunlock(tdb, ltype, TDB1_FREELIST_TOP,
			     tdb->header.hash_size * 4);
		return -1;
	}

	tdb->allrecord_lock.count = 1;
	/* If it's upgradable, it's actually exclusive so we can treat
	 * it as a write lock. */
	tdb->allrecord_lock.ltype = upgradable ? F_WRLCK : ltype;
	tdb->allrecord_lock.off = upgradable;

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
int tdb1_allrecord_unlock(struct tdb1_context *tdb, int ltype)
{
	/* There are no locks on read-only dbs */
	if (tdb->read_only || tdb->traverse_read) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->allrecord_lock.count == 0) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	/* Upgradable locks are marked as write locks. */
	if (tdb->allrecord_lock.ltype != ltype
	    && (!tdb->allrecord_lock.off || ltype != F_RDLCK)) {
		tdb->last_error = TDB_ERR_LOCK;
		return -1;
	}

	if (tdb->allrecord_lock.count > 1) {
		tdb->allrecord_lock.count--;
		return 0;
	}

	if (tdb1_brunlock(tdb, ltype, TDB1_FREELIST_TOP, 0)) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_unlockall failed (%s)", strerror(errno));
		return -1;
	}

	tdb->allrecord_lock.count = 0;
	tdb->allrecord_lock.ltype = 0;

	return 0;
}

/* lock entire database with write lock */
int tdb1_lockall(struct tdb1_context *tdb)
{
	return tdb1_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false);
}

/* unlock entire database with write lock */
int tdb1_unlockall(struct tdb1_context *tdb)
{
	return tdb1_allrecord_unlock(tdb, F_WRLCK);
}

/* lock entire database with read lock */
int tdb1_lockall_read(struct tdb1_context *tdb)
{
	return tdb1_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false);
}

/* unlock entire database with read lock */
int tdb1_unlockall_read(struct tdb1_context *tdb)
{
	return tdb1_allrecord_unlock(tdb, F_RDLCK);
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
int tdb1_chainlock(struct tdb1_context *tdb, TDB_DATA key)
{
	int ret = tdb1_lock(tdb, TDB1_BUCKET(tdb->hash_fn(&key)), F_WRLCK);
	return ret;
}

int tdb1_chainunlock(struct tdb1_context *tdb, TDB_DATA key)
{
	return tdb1_unlock(tdb, TDB1_BUCKET(tdb->hash_fn(&key)), F_WRLCK);
}

int tdb1_chainlock_read(struct tdb1_context *tdb, TDB_DATA key)
{
	int ret;
	ret = tdb1_lock(tdb, TDB1_BUCKET(tdb->hash_fn(&key)), F_RDLCK);
	return ret;
}

int tdb1_chainunlock_read(struct tdb1_context *tdb, TDB_DATA key)
{
	return tdb1_unlock(tdb, TDB1_BUCKET(tdb->hash_fn(&key)), F_RDLCK);
}

/* record lock stops delete underneath */
int tdb1_lock_record(struct tdb1_context *tdb, tdb1_off_t off)
{
	if (tdb->allrecord_lock.count) {
		return 0;
	}
	return off ? tdb1_brlock(tdb, F_RDLCK, off, 1, TDB_LOCK_WAIT) : 0;
}

/*
  Write locks override our own fcntl readlocks, so check it here.
  Note this is meant to be F_SETLK, *not* F_SETLKW, as it's not
  an error to fail to get the lock here.
*/
int tdb1_write_lock_record(struct tdb1_context *tdb, tdb1_off_t off)
{
	struct tdb1_traverse_lock *i;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			return -1;
	if (tdb->allrecord_lock.count) {
		if (tdb->allrecord_lock.ltype == F_WRLCK) {
			return 0;
		}
		return -1;
	}
	return tdb1_brlock(tdb, F_WRLCK, off, 1, TDB_LOCK_NOWAIT|TDB_LOCK_PROBE);
}

int tdb1_write_unlock_record(struct tdb1_context *tdb, tdb1_off_t off)
{
	if (tdb->allrecord_lock.count) {
		return 0;
	}
	return tdb1_brunlock(tdb, F_WRLCK, off, 1);
}

/* fcntl locks don't stack: avoid unlocking someone else's */
int tdb1_unlock_record(struct tdb1_context *tdb, tdb1_off_t off)
{
	struct tdb1_traverse_lock *i;
	uint32_t count = 0;

	if (tdb->allrecord_lock.count) {
		return 0;
	}

	if (off == 0)
		return 0;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb1_brunlock(tdb, F_RDLCK, off, 1) : 0);
}

bool tdb1_have_extra_locks(struct tdb1_context *tdb)
{
	unsigned int extra = tdb->num_lockrecs;

	/* A transaction holds the lock for all records. */
	if (!tdb->transaction && tdb->allrecord_lock.count) {
		return true;
	}

	/* We always hold the active lock if CLEAR_IF_FIRST. */
	if (tdb1_find_nestlock(tdb, TDB1_ACTIVE_LOCK)) {
		extra--;
	}

	/* In a transaction, we expect to hold the transaction lock */
	if (tdb->transaction
	    && tdb1_find_nestlock(tdb, TDB1_TRANSACTION_LOCK)) {
		extra--;
	}

	return extra;
}

/* The transaction code uses this to remove all locks. */
void tdb1_release_transaction_locks(struct tdb1_context *tdb)
{
	unsigned int i, active = 0;

	if (tdb->allrecord_lock.count != 0) {
		tdb1_brunlock(tdb, tdb->allrecord_lock.ltype, TDB1_FREELIST_TOP, 0);
		tdb->allrecord_lock.count = 0;
	}

	for (i=0;i<tdb->num_lockrecs;i++) {
		struct tdb1_lock_type *lck = &tdb->lockrecs[i];

		/* Don't release the active lock!  Copy it to first entry. */
		if (lck->off == TDB1_ACTIVE_LOCK) {
			tdb->lockrecs[active++] = *lck;
		} else {
			tdb1_brunlock(tdb, lck->ltype, lck->off, 1);
		}
	}
	tdb->num_lockrecs = active;
	if (tdb->num_lockrecs == 0) {
		SAFE_FREE(tdb->lockrecs);
	}
}
