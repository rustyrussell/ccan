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

#include "private.h"
#include <assert.h>
#include <ccan/build_assert/build_assert.h>

/* If we were threaded, we could wait for unlock, but we're not, so fail. */
static enum TDB_ERROR owner_conflict(struct tdb_context *tdb, const char *call)
{
	return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
			  "%s: lock owned by another tdb in this process.",
			  call);
}

/* If we fork, we no longer really own locks. */
static bool check_lock_pid(struct tdb_context *tdb,
			   const char *call, bool log)
{
	/* No locks?  No problem! */
	if (tdb->file->allrecord_lock.count == 0
	    && tdb->file->num_lockrecs == 0) {
		return true;
	}

	/* No fork?  No problem! */
	if (tdb->file->locker == getpid()) {
		return true;
	}

	if (log) {
		tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
			   "%s: fork() detected after lock acquisition!"
			   " (%u vs %u)", call, tdb->file->locker, getpid());
	}
	return false;
}

int tdb_fcntl_lock(int fd, int rw, off_t off, off_t len, bool waitflag,
		   void *unused)
{
	struct flock fl;
	int ret;

	do {
		fl.l_type = rw;
		fl.l_whence = SEEK_SET;
		fl.l_start = off;
		fl.l_len = len;

		if (waitflag)
			ret = fcntl(fd, F_SETLKW, &fl);
		else
			ret = fcntl(fd, F_SETLK, &fl);
	} while (ret != 0 && errno == EINTR);
	return ret;
}

int tdb_fcntl_unlock(int fd, int rw, off_t off, off_t len, void *unused)
{
	struct flock fl;
	int ret;

	do {
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = off;
		fl.l_len = len;

		ret = fcntl(fd, F_SETLKW, &fl);
	} while (ret != 0 && errno == EINTR);
	return ret;
}

static int lock(struct tdb_context *tdb,
		      int rw, off_t off, off_t len, bool waitflag)
{
	int ret;
	if (tdb->file->allrecord_lock.count == 0
	    && tdb->file->num_lockrecs == 0) {
		tdb->file->locker = getpid();
	}

	tdb->stats.lock_lowlevel++;
	ret = tdb->lock_fn(tdb->file->fd, rw, off, len, waitflag,
			   tdb->lock_data);
	if (!waitflag) {
		tdb->stats.lock_nonblock++;
		if (ret != 0)
			tdb->stats.lock_nonblock_fail++;
	}
	return ret;
}

static int unlock(struct tdb_context *tdb, int rw, off_t off, off_t len)
{
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
		fprintf(stderr, "Unlock on %u@%u not found!",
			(int)off, (int)len);
		abort();
	}

	fclose(locks);
#endif

	return tdb->unlock_fn(tdb->file->fd, rw, off, len, tdb->lock_data);
}

/* a byte range locking function - return 0 on success
   this functions locks len bytes at the specified offset.

   note that a len of zero means lock to end of file
*/
static enum TDB_ERROR tdb_brlock(struct tdb_context *tdb,
				 int rw_type, tdb_off_t offset, tdb_off_t len,
				 enum tdb_lock_flags flags)
{
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return TDB_SUCCESS;
	}

	if (rw_type == F_WRLCK && tdb->read_only) {
		return tdb_logerr(tdb, TDB_ERR_RDONLY, TDB_LOG_USE_ERROR,
				  "Write lock attempted on read-only database");
	}

	/* A 32 bit system cannot open a 64-bit file, but it could have
	 * expanded since then: check here. */
	if ((size_t)(offset + len) != offset + len) {
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "tdb_brlock: lock on giant offset %llu",
				  (long long)(offset + len));
	}

	ret = lock(tdb, rw_type, offset, len, flags & TDB_LOCK_WAIT);
	if (ret != 0) {
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!(flags & TDB_LOCK_PROBE)
		    && (errno != EAGAIN && errno != EINTR)) {
			tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				   "tdb_brlock failed (fd=%d) at"
				   " offset %zu rw_type=%d flags=%d len=%zu:"
				   " %s",
				   tdb->file->fd, (size_t)offset, rw_type,
				   flags, (size_t)len, strerror(errno));
		}
		return TDB_ERR_LOCK;
	}
	return TDB_SUCCESS;
}

static enum TDB_ERROR tdb_brunlock(struct tdb_context *tdb,
				   int rw_type, tdb_off_t offset, size_t len)
{
	if (tdb->flags & TDB_NOLOCK) {
		return TDB_SUCCESS;
	}

	if (!check_lock_pid(tdb, "tdb_brunlock", true))
		return TDB_ERR_LOCK;

	if (unlock(tdb, rw_type, offset, len) == -1) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_brunlock failed (fd=%d) at offset %zu"
				  " rw_type=%d len=%zu: %s",
				  tdb->file->fd, (size_t)offset, rw_type,
				  (size_t)len, strerror(errno));
	}
	return TDB_SUCCESS;
}

/*
  upgrade a read lock to a write lock. This needs to be handled in a
  special way as some OSes (such as solaris) have too conservative
  deadlock detection and claim a deadlock when progress can be
  made. For those OSes we may loop for a while.
*/
enum TDB_ERROR tdb_allrecord_upgrade(struct tdb_context *tdb)
{
	int count = 1000;

	if (!check_lock_pid(tdb, "tdb_transaction_prepare_commit", true))
		return TDB_ERR_LOCK;

	if (tdb->file->allrecord_lock.count != 1) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_allrecord_upgrade failed:"
				  " count %u too high",
				  tdb->file->allrecord_lock.count);
	}

	if (tdb->file->allrecord_lock.off != 1) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_allrecord_upgrade failed:"
				  " already upgraded?");
	}

	if (tdb->file->allrecord_lock.owner != tdb) {
		return owner_conflict(tdb, "tdb_allrecord_upgrade");
	}

	while (count--) {
		struct timeval tv;
		if (tdb_brlock(tdb, F_WRLCK,
			       TDB_HASH_LOCK_START, 0,
			       TDB_LOCK_WAIT|TDB_LOCK_PROBE) == TDB_SUCCESS) {
			tdb->file->allrecord_lock.ltype = F_WRLCK;
			tdb->file->allrecord_lock.off = 0;
			return TDB_SUCCESS;
		}
		if (errno != EDEADLK) {
			break;
		}
		/* sleep for as short a time as we can - more portable than usleep() */
		tv.tv_sec = 0;
		tv.tv_usec = 1;
		select(0, NULL, NULL, NULL, &tv);
	}

	if (errno != EAGAIN && errno != EINTR)
		tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
			   "tdb_allrecord_upgrade failed");
	return TDB_ERR_LOCK;
}

static struct tdb_lock *find_nestlock(struct tdb_context *tdb, tdb_off_t offset,
				      const struct tdb_context *owner)
{
	unsigned int i;

	for (i=0; i<tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].off == offset) {
			if (owner && tdb->file->lockrecs[i].owner != owner)
				return NULL;
			return &tdb->file->lockrecs[i];
		}
	}
	return NULL;
}

enum TDB_ERROR tdb_lock_and_recover(struct tdb_context *tdb)
{
	enum TDB_ERROR ecode;

	if (!check_lock_pid(tdb, "tdb_transaction_prepare_commit", true))
		return TDB_ERR_LOCK;

	ecode = tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT|TDB_LOCK_NOCHECK,
				   false);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	ecode = tdb_lock_open(tdb, F_WRLCK, TDB_LOCK_WAIT|TDB_LOCK_NOCHECK);
	if (ecode != TDB_SUCCESS) {
		tdb_allrecord_unlock(tdb, F_WRLCK);
		return ecode;
	}
	ecode = tdb_transaction_recover(tdb);
	tdb_unlock_open(tdb, F_WRLCK);
	tdb_allrecord_unlock(tdb, F_WRLCK);

	return ecode;
}

/* lock an offset in the database. */
static enum TDB_ERROR tdb_nest_lock(struct tdb_context *tdb,
				    tdb_off_t offset, int ltype,
				    enum tdb_lock_flags flags)
{
	struct tdb_lock *new_lck;
	enum TDB_ERROR ecode;

	if (offset > (TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE
		      + tdb->file->map_size / 8)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_nest_lock: invalid offset %zu ltype=%d",
				  (size_t)offset, ltype);
	}

	if (tdb->flags & TDB_NOLOCK)
		return TDB_SUCCESS;

	if (!check_lock_pid(tdb, "tdb_nest_lock", true)) {
		return TDB_ERR_LOCK;
	}

	tdb->stats.locks++;

	new_lck = find_nestlock(tdb, offset, NULL);
	if (new_lck) {
		if (new_lck->owner != tdb) {
			return owner_conflict(tdb, "tdb_nest_lock");
		}

		if (new_lck->ltype == F_RDLCK && ltype == F_WRLCK) {
			return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					  "tdb_nest_lock:"
					  " offset %zu has read lock",
					  (size_t)offset);
		}
		/* Just increment the struct, posix locks don't stack. */
		new_lck->count++;
		return TDB_SUCCESS;
	}

#if 0
	if (tdb->file->num_lockrecs
	    && offset >= TDB_HASH_LOCK_START
	    && offset < TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_nest_lock: already have a hash lock?");
	}
#endif

	new_lck = (struct tdb_lock *)realloc(
		tdb->file->lockrecs,
		sizeof(*tdb->file->lockrecs) * (tdb->file->num_lockrecs+1));
	if (new_lck == NULL) {
		return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
				  "tdb_nest_lock:"
				  " unable to allocate %zu lock struct",
				  tdb->file->num_lockrecs + 1);
	}
	tdb->file->lockrecs = new_lck;

	/* Since fcntl locks don't nest, we do a lock for the first one,
	   and simply bump the count for future ones */
	ecode = tdb_brlock(tdb, ltype, offset, 1, flags);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* First time we grab a lock, perhaps someone died in commit? */
	if (!(flags & TDB_LOCK_NOCHECK)
	    && tdb->file->num_lockrecs == 0) {
		tdb_bool_err berr = tdb_needs_recovery(tdb);
		if (berr != false) {
			tdb_brunlock(tdb, ltype, offset, 1);

			if (berr < 0)
				return berr;
			ecode = tdb_lock_and_recover(tdb);
			if (ecode == TDB_SUCCESS) {
				ecode = tdb_brlock(tdb, ltype, offset, 1,
						   flags);
			}
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
		}
	}

	tdb->file->lockrecs[tdb->file->num_lockrecs].owner = tdb;
	tdb->file->lockrecs[tdb->file->num_lockrecs].off = offset;
	tdb->file->lockrecs[tdb->file->num_lockrecs].count = 1;
	tdb->file->lockrecs[tdb->file->num_lockrecs].ltype = ltype;
	tdb->file->num_lockrecs++;

	return TDB_SUCCESS;
}

static enum TDB_ERROR tdb_nest_unlock(struct tdb_context *tdb,
				      tdb_off_t off, int ltype)
{
	struct tdb_lock *lck;
	enum TDB_ERROR ecode;

	if (tdb->flags & TDB_NOLOCK)
		return TDB_SUCCESS;

	lck = find_nestlock(tdb, off, tdb);
	if ((lck == NULL) || (lck->count == 0)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_nest_unlock: no lock for %zu",
				  (size_t)off);
	}

	if (lck->count > 1) {
		lck->count--;
		return TDB_SUCCESS;
	}

	/*
	 * This lock has count==1 left, so we need to unlock it in the
	 * kernel. We don't bother with decrementing the in-memory array
	 * element, we're about to overwrite it with the last array element
	 * anyway.
	 */
	ecode = tdb_brunlock(tdb, ltype, off, 1);

	/*
	 * Shrink the array by overwriting the element just unlocked with the
	 * last array element.
	 */
	*lck = tdb->file->lockrecs[--tdb->file->num_lockrecs];

	return ecode;
}

/*
  get the transaction lock
 */
enum TDB_ERROR tdb_transaction_lock(struct tdb_context *tdb, int ltype)
{
	return tdb_nest_lock(tdb, TDB_TRANSACTION_LOCK, ltype, TDB_LOCK_WAIT);
}

/*
  release the transaction lock
 */
void tdb_transaction_unlock(struct tdb_context *tdb, int ltype)
{
	tdb_nest_unlock(tdb, TDB_TRANSACTION_LOCK, ltype);
}

/* We only need to lock individual bytes, but Linux merges consecutive locks
 * so we lock in contiguous ranges. */
static enum TDB_ERROR tdb_lock_gradual(struct tdb_context *tdb,
				       int ltype, enum tdb_lock_flags flags,
				       tdb_off_t off, tdb_off_t len)
{
	enum TDB_ERROR ecode;
	enum tdb_lock_flags nb_flags = (flags & ~TDB_LOCK_WAIT);

	if (len <= 1) {
		/* 0 would mean to end-of-file... */
		assert(len != 0);
		/* Single hash.  Just do blocking lock. */
		return tdb_brlock(tdb, ltype, off, len, flags);
	}

	/* First we try non-blocking. */
	if (tdb_brlock(tdb, ltype, off, len, nb_flags) == TDB_SUCCESS) {
		return TDB_SUCCESS;
	}

	/* Try locking first half, then second. */
	ecode = tdb_lock_gradual(tdb, ltype, flags, off, len / 2);
	if (ecode != TDB_SUCCESS)
		return ecode;

	ecode = tdb_lock_gradual(tdb, ltype, flags,
				 off + len / 2, len - len / 2);
	if (ecode != TDB_SUCCESS) {
		tdb_brunlock(tdb, ltype, off, len / 2);
	}
	return ecode;
}

/* lock/unlock entire database.  It can only be upgradable if you have some
 * other way of guaranteeing exclusivity (ie. transaction write lock). */
enum TDB_ERROR tdb_allrecord_lock(struct tdb_context *tdb, int ltype,
				  enum tdb_lock_flags flags, bool upgradable)
{
	enum TDB_ERROR ecode;
	tdb_bool_err berr;

	if (tdb->flags & TDB_NOLOCK)
		return TDB_SUCCESS;

	if (!check_lock_pid(tdb, "tdb_allrecord_lock", true)) {
		return TDB_ERR_LOCK;
	}

	if (tdb->file->allrecord_lock.count) {
		if (tdb->file->allrecord_lock.owner != tdb) {
			return owner_conflict(tdb, "tdb_allrecord_lock");
		}

		if (ltype == F_RDLCK
		    || tdb->file->allrecord_lock.ltype == F_WRLCK) {
			tdb->file->allrecord_lock.count++;
			return TDB_SUCCESS;
		}

		/* a global lock of a different type exists */
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
				  "tdb_allrecord_lock: already have %s lock",
				  tdb->file->allrecord_lock.ltype == F_RDLCK
				  ? "read" : "write");
	}

	if (tdb_has_hash_locks(tdb)) {
		/* can't combine global and chain locks */
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
				  "tdb_allrecord_lock:"
				  " already have chain lock");
	}

	if (upgradable && ltype != F_RDLCK) {
		/* tdb error: you can't upgrade a write lock! */
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_allrecord_lock:"
				  " can't upgrade a write lock");
	}

	tdb->stats.locks++;
again:
	/* Lock hashes, gradually. */
	ecode = tdb_lock_gradual(tdb, ltype, flags, TDB_HASH_LOCK_START,
				 TDB_HASH_LOCK_RANGE);
	if (ecode != TDB_SUCCESS)
		return ecode;

	/* Lock free tables: there to end of file. */
	ecode = tdb_brlock(tdb, ltype,
			   TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE,
			   0, flags);
	if (ecode != TDB_SUCCESS) {
		tdb_brunlock(tdb, ltype, TDB_HASH_LOCK_START,
			     TDB_HASH_LOCK_RANGE);
		return ecode;
	}

	tdb->file->allrecord_lock.owner = tdb;
	tdb->file->allrecord_lock.count = 1;
	/* If it's upgradable, it's actually exclusive so we can treat
	 * it as a write lock. */
	tdb->file->allrecord_lock.ltype = upgradable ? F_WRLCK : ltype;
	tdb->file->allrecord_lock.off = upgradable;

	/* Now check for needing recovery. */
	if (flags & TDB_LOCK_NOCHECK)
		return TDB_SUCCESS;

	berr = tdb_needs_recovery(tdb);
	if (likely(berr == false))
		return TDB_SUCCESS;

	tdb_allrecord_unlock(tdb, ltype);
	if (berr < 0)
		return berr;
	ecode = tdb_lock_and_recover(tdb);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}
	goto again;
}

enum TDB_ERROR tdb_lock_open(struct tdb_context *tdb,
			     int ltype, enum tdb_lock_flags flags)
{
	return tdb_nest_lock(tdb, TDB_OPEN_LOCK, ltype, flags);
}

void tdb_unlock_open(struct tdb_context *tdb, int ltype)
{
	tdb_nest_unlock(tdb, TDB_OPEN_LOCK, ltype);
}

bool tdb_has_open_lock(struct tdb_context *tdb)
{
	return !(tdb->flags & TDB_NOLOCK)
		&& find_nestlock(tdb, TDB_OPEN_LOCK, tdb) != NULL;
}

enum TDB_ERROR tdb_lock_expand(struct tdb_context *tdb, int ltype)
{
	/* Lock doesn't protect data, so don't check (we recurse if we do!) */
	return tdb_nest_lock(tdb, TDB_EXPANSION_LOCK, ltype,
			     TDB_LOCK_WAIT | TDB_LOCK_NOCHECK);
}

void tdb_unlock_expand(struct tdb_context *tdb, int ltype)
{
	tdb_nest_unlock(tdb, TDB_EXPANSION_LOCK, ltype);
}

/* unlock entire db */
void tdb_allrecord_unlock(struct tdb_context *tdb, int ltype)
{
	if (tdb->flags & TDB_NOLOCK)
		return;

	if (tdb->file->allrecord_lock.count == 0) {
		tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
			   "tdb_allrecord_unlock: not locked!");
		return;
	}

	if (tdb->file->allrecord_lock.owner != tdb) {
		tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
			   "tdb_allrecord_unlock: not locked by us!");
		return;
	}

	/* Upgradable locks are marked as write locks. */
	if (tdb->file->allrecord_lock.ltype != ltype
	    && (!tdb->file->allrecord_lock.off || ltype != F_RDLCK)) {
		tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
			   "tdb_allrecord_unlock: have %s lock",
			   tdb->file->allrecord_lock.ltype == F_RDLCK
			   ? "read" : "write");
		return;
	}

	if (tdb->file->allrecord_lock.count > 1) {
		tdb->file->allrecord_lock.count--;
		return;
	}

	tdb->file->allrecord_lock.count = 0;
	tdb->file->allrecord_lock.ltype = 0;

	tdb_brunlock(tdb, ltype, TDB_HASH_LOCK_START, 0);
}

bool tdb_has_expansion_lock(struct tdb_context *tdb)
{
	return find_nestlock(tdb, TDB_EXPANSION_LOCK, tdb) != NULL;
}

bool tdb_has_hash_locks(struct tdb_context *tdb)
{
	unsigned int i;

	for (i=0; i<tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].off >= TDB_HASH_LOCK_START
		    && tdb->file->lockrecs[i].off < (TDB_HASH_LOCK_START
						     + TDB_HASH_LOCK_RANGE))
			return true;
	}
	return false;
}

static bool tdb_has_free_lock(struct tdb_context *tdb)
{
	unsigned int i;

	if (tdb->flags & TDB_NOLOCK)
		return false;

	for (i=0; i<tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].off
		    > TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE)
			return true;
	}
	return false;
}

enum TDB_ERROR tdb_lock_hashes(struct tdb_context *tdb,
			       tdb_off_t hash_lock,
			       tdb_len_t hash_range,
			       int ltype, enum tdb_lock_flags waitflag)
{
	/* FIXME: Do this properly, using hlock_range */
	unsigned l = TDB_HASH_LOCK_START
		+ (hash_lock >> (64 - TDB_HASH_LOCK_RANGE_BITS));

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->file->allrecord_lock.count) {
		if (!check_lock_pid(tdb, "tdb_lock_hashes", true))
			return TDB_ERR_LOCK;

		if (tdb->file->allrecord_lock.owner != tdb)
			return owner_conflict(tdb, "tdb_lock_hashes");
		if (ltype == tdb->file->allrecord_lock.ltype
		    || ltype == F_RDLCK) {
			return TDB_SUCCESS;
		}

		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_USE_ERROR,
				  "tdb_lock_hashes:"
				  " already have %s allrecordlock",
				  tdb->file->allrecord_lock.ltype == F_RDLCK
				  ? "read" : "write");
	}

	if (tdb_has_free_lock(tdb)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_lock_hashes: already have free lock");
	}

	if (tdb_has_expansion_lock(tdb)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_lock_hashes:"
				  " already have expansion lock");
	}

	return tdb_nest_lock(tdb, l, ltype, waitflag);
}

enum TDB_ERROR tdb_unlock_hashes(struct tdb_context *tdb,
				 tdb_off_t hash_lock,
				 tdb_len_t hash_range, int ltype)
{
	unsigned l = TDB_HASH_LOCK_START
		+ (hash_lock >> (64 - TDB_HASH_LOCK_RANGE_BITS));

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->file->allrecord_lock.count) {
		if (tdb->file->allrecord_lock.ltype == F_RDLCK
		    && ltype == F_WRLCK) {
			return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
					  "tdb_unlock_hashes RO allrecord!");
		}
		return TDB_SUCCESS;
	}

	return tdb_nest_unlock(tdb, l, ltype);
}

/* Hash locks use TDB_HASH_LOCK_START + the next 30 bits.
 * Then we begin; bucket offsets are sizeof(tdb_len_t) apart, so we divide.
 * The result is that on 32 bit systems we don't use lock values > 2^31 on
 * files that are less than 4GB.
 */
static tdb_off_t free_lock_off(tdb_off_t b_off)
{
	return TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE
		+ b_off / sizeof(tdb_off_t);
}

enum TDB_ERROR tdb_lock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off,
				    enum tdb_lock_flags waitflag)
{
	assert(b_off >= sizeof(struct tdb_header));

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->file->allrecord_lock.count) {
		if (!check_lock_pid(tdb, "tdb_lock_free_bucket", true))
			return TDB_ERR_LOCK;

		if (tdb->file->allrecord_lock.ltype == F_WRLCK)
			return 0;
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_lock_free_bucket with"
				  " read-only allrecordlock!");
	}

#if 0 /* FIXME */
	if (tdb_has_expansion_lock(tdb)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_lock_free_bucket:"
				  " already have expansion lock");
	}
#endif

	return tdb_nest_lock(tdb, free_lock_off(b_off), F_WRLCK, waitflag);
}

void tdb_unlock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off)
{
	if (tdb->file->allrecord_lock.count)
		return;

	tdb_nest_unlock(tdb, free_lock_off(b_off), F_WRLCK);
}

enum TDB_ERROR tdb_lockall(struct tdb_context *tdb)
{
	return tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false);
}

void tdb_unlockall(struct tdb_context *tdb)
{
	tdb_allrecord_unlock(tdb, F_WRLCK);
}

enum TDB_ERROR tdb_lockall_read(struct tdb_context *tdb)
{
	return tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false);
}

void tdb_unlockall_read(struct tdb_context *tdb)
{
	tdb_allrecord_unlock(tdb, F_RDLCK);
}

void tdb_lock_cleanup(struct tdb_context *tdb)
{
	unsigned int i;

	/* We don't want to warn: they're allowed to close tdb after fork. */
	if (!check_lock_pid(tdb, "tdb_close", false))
		return;

	while (tdb->file->allrecord_lock.count
	       && tdb->file->allrecord_lock.owner == tdb) {
		tdb_allrecord_unlock(tdb, tdb->file->allrecord_lock.ltype);
	}

	for (i=0; i<tdb->file->num_lockrecs; i++) {
		if (tdb->file->lockrecs[i].owner == tdb) {
			tdb_nest_unlock(tdb,
					tdb->file->lockrecs[i].off,
					tdb->file->lockrecs[i].ltype);
			i--;
		}
	}
}
