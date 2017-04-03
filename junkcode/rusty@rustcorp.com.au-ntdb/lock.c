 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003

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
#include <ccan/build_assert/build_assert.h>

/* If we were threaded, we could wait for unlock, but we're not, so fail. */
enum NTDB_ERROR owner_conflict(struct ntdb_context *ntdb, const char *call)
{
	return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
			  "%s: lock owned by another ntdb in this process.",
			  call);
}

/* If we fork, we no longer really own locks. */
bool check_lock_pid(struct ntdb_context *ntdb, const char *call, bool log)
{
	/* No locks?  No problem! */
	if (ntdb->file->allrecord_lock.count == 0
	    && ntdb->file->num_lockrecs == 0) {
		return true;
	}

	/* No fork?  No problem! */
	if (ntdb->file->locker == getpid()) {
		return true;
	}

	if (log) {
		ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
			    "%s: fork() detected after lock acquisition!"
			    " (%u vs %u)", call,
			    (unsigned int)ntdb->file->locker,
			    (unsigned int)getpid());
	}
	return false;
}

int ntdb_fcntl_lock(int fd, int rw, off_t off, off_t len, bool waitflag,
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

int ntdb_fcntl_unlock(int fd, int rw, off_t off, off_t len, void *unused)
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

static int lock(struct ntdb_context *ntdb,
		      int rw, off_t off, off_t len, bool waitflag)
{
	int ret;
	if (ntdb->file->allrecord_lock.count == 0
	    && ntdb->file->num_lockrecs == 0) {
		ntdb->file->locker = getpid();
	}

	ntdb->stats.lock_lowlevel++;
	ret = ntdb->lock_fn(ntdb->file->fd, rw, off, len, waitflag,
			   ntdb->lock_data);
	if (!waitflag) {
		ntdb->stats.lock_nonblock++;
		if (ret != 0)
			ntdb->stats.lock_nonblock_fail++;
	}
	return ret;
}

static int unlock(struct ntdb_context *ntdb, int rw, off_t off, off_t len)
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

	return ntdb->unlock_fn(ntdb->file->fd, rw, off, len, ntdb->lock_data);
}

/* a byte range locking function - return 0 on success
   this functions locks len bytes at the specified offset.

   note that a len of zero means lock to end of file
*/
static enum NTDB_ERROR ntdb_brlock(struct ntdb_context *ntdb,
				 int rw_type, ntdb_off_t offset, ntdb_off_t len,
				 enum ntdb_lock_flags flags)
{
	int ret;

	if (rw_type == F_WRLCK && (ntdb->flags & NTDB_RDONLY)) {
		return ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_USE_ERROR,
				  "Write lock attempted on read-only database");
	}

	if (ntdb->flags & NTDB_NOLOCK) {
		return NTDB_SUCCESS;
	}

	/* A 32 bit system cannot open a 64-bit file, but it could have
	 * expanded since then: check here. */
	if ((size_t)(offset + len) != offset + len) {
		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "ntdb_brlock: lock on giant offset %llu",
				  (long long)(offset + len));
	}

	ret = lock(ntdb, rw_type, offset, len, flags & NTDB_LOCK_WAIT);
	if (ret != 0) {
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!(flags & NTDB_LOCK_PROBE)
		    && (errno != EAGAIN && errno != EINTR)) {
			ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				   "ntdb_brlock failed (fd=%d) at"
				   " offset %zu rw_type=%d flags=%d len=%zu:"
				   " %s",
				   ntdb->file->fd, (size_t)offset, rw_type,
				   flags, (size_t)len, strerror(errno));
		}
		return NTDB_ERR_LOCK;
	}
	return NTDB_SUCCESS;
}

static enum NTDB_ERROR ntdb_brunlock(struct ntdb_context *ntdb,
				   int rw_type, ntdb_off_t offset, size_t len)
{
	if (ntdb->flags & NTDB_NOLOCK) {
		return NTDB_SUCCESS;
	}

	if (!check_lock_pid(ntdb, "ntdb_brunlock", false))
		return NTDB_ERR_LOCK;

	if (unlock(ntdb, rw_type, offset, len) == -1) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_brunlock failed (fd=%d) at offset %zu"
				  " rw_type=%d len=%zu: %s",
				  ntdb->file->fd, (size_t)offset, rw_type,
				  (size_t)len, strerror(errno));
	}
	return NTDB_SUCCESS;
}

/*
  upgrade a read lock to a write lock. This needs to be handled in a
  special way as some OSes (such as solaris) have too conservative
  deadlock detection and claim a deadlock when progress can be
  made. For those OSes we may loop for a while.
*/
enum NTDB_ERROR ntdb_allrecord_upgrade(struct ntdb_context *ntdb, off_t start)
{
	int count = 1000;

	if (!check_lock_pid(ntdb, "ntdb_transaction_prepare_commit", true))
		return NTDB_ERR_LOCK;

	if (ntdb->file->allrecord_lock.count != 1) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_allrecord_upgrade failed:"
				  " count %u too high",
				  ntdb->file->allrecord_lock.count);
	}

	if (ntdb->file->allrecord_lock.off != 1) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_allrecord_upgrade failed:"
				  " already upgraded?");
	}

	if (ntdb->file->allrecord_lock.owner != ntdb) {
		return owner_conflict(ntdb, "ntdb_allrecord_upgrade");
	}

	while (count--) {
		struct timeval tv;
		if (ntdb_brlock(ntdb, F_WRLCK, start, 0,
			       NTDB_LOCK_WAIT|NTDB_LOCK_PROBE) == NTDB_SUCCESS) {
			ntdb->file->allrecord_lock.ltype = F_WRLCK;
			ntdb->file->allrecord_lock.off = 0;
			return NTDB_SUCCESS;
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
		ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
			   "ntdb_allrecord_upgrade failed");
	return NTDB_ERR_LOCK;
}

static struct ntdb_lock *find_nestlock(struct ntdb_context *ntdb, ntdb_off_t offset,
				      const struct ntdb_context *owner)
{
	unsigned int i;

	for (i=0; i<ntdb->file->num_lockrecs; i++) {
		if (ntdb->file->lockrecs[i].off == offset) {
			if (owner && ntdb->file->lockrecs[i].owner != owner)
				return NULL;
			return &ntdb->file->lockrecs[i];
		}
	}
	return NULL;
}

enum NTDB_ERROR ntdb_lock_and_recover(struct ntdb_context *ntdb)
{
	enum NTDB_ERROR ecode;

	if (!check_lock_pid(ntdb, "ntdb_transaction_prepare_commit", true))
		return NTDB_ERR_LOCK;

	ecode = ntdb_allrecord_lock(ntdb, F_WRLCK, NTDB_LOCK_WAIT|NTDB_LOCK_NOCHECK,
				   false);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	ecode = ntdb_lock_open(ntdb, F_WRLCK, NTDB_LOCK_WAIT|NTDB_LOCK_NOCHECK);
	if (ecode != NTDB_SUCCESS) {
		ntdb_allrecord_unlock(ntdb, F_WRLCK);
		return ecode;
	}
	ecode = ntdb_transaction_recover(ntdb);
	ntdb_unlock_open(ntdb, F_WRLCK);
	ntdb_allrecord_unlock(ntdb, F_WRLCK);

	return ecode;
}

/* lock an offset in the database. */
static enum NTDB_ERROR ntdb_nest_lock(struct ntdb_context *ntdb,
				    ntdb_off_t offset, int ltype,
				    enum ntdb_lock_flags flags)
{
	struct ntdb_lock *new_lck;
	enum NTDB_ERROR ecode;

	assert(offset <= (NTDB_HASH_LOCK_START + (1 << ntdb->hash_bits)
			  + ntdb->file->map_size / 8));

	if (ntdb->flags & NTDB_NOLOCK)
		return NTDB_SUCCESS;

	if (!check_lock_pid(ntdb, "ntdb_nest_lock", true)) {
		return NTDB_ERR_LOCK;
	}

	ntdb->stats.locks++;

	new_lck = find_nestlock(ntdb, offset, NULL);
	if (new_lck) {
		if (new_lck->owner != ntdb) {
			return owner_conflict(ntdb, "ntdb_nest_lock");
		}

		if (new_lck->ltype == F_RDLCK && ltype == F_WRLCK) {
			return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
					  "ntdb_nest_lock:"
					  " offset %zu has read lock",
					  (size_t)offset);
		}
		/* Just increment the struct, posix locks don't stack. */
		new_lck->count++;
		return NTDB_SUCCESS;
	}

#if 0
	if (ntdb->file->num_lockrecs
	    && offset >= NTDB_HASH_LOCK_START
	    && offset < NTDB_HASH_LOCK_START + NTDB_HASH_LOCK_RANGE) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_nest_lock: already have a hash lock?");
	}
#endif
	if (ntdb->file->lockrecs == NULL) {
		new_lck = ntdb->alloc_fn(ntdb->file, sizeof(*ntdb->file->lockrecs),
				     ntdb->alloc_data);
	} else {
		new_lck = (struct ntdb_lock *)ntdb->expand_fn(
			ntdb->file->lockrecs,
			sizeof(*ntdb->file->lockrecs)
			* (ntdb->file->num_lockrecs+1),
			ntdb->alloc_data);
	}
	if (new_lck == NULL) {
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				  "ntdb_nest_lock:"
				  " unable to allocate %zu lock struct",
				  ntdb->file->num_lockrecs + 1);
	}
	ntdb->file->lockrecs = new_lck;

	/* Since fcntl locks don't nest, we do a lock for the first one,
	   and simply bump the count for future ones */
	ecode = ntdb_brlock(ntdb, ltype, offset, 1, flags);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* First time we grab a lock, perhaps someone died in commit? */
	if (!(flags & NTDB_LOCK_NOCHECK)
	    && ntdb->file->num_lockrecs == 0) {
		ntdb_bool_err berr = ntdb_needs_recovery(ntdb);
		if (berr != false) {
			ntdb_brunlock(ntdb, ltype, offset, 1);

			if (berr < 0)
				return NTDB_OFF_TO_ERR(berr);
			ecode = ntdb_lock_and_recover(ntdb);
			if (ecode == NTDB_SUCCESS) {
				ecode = ntdb_brlock(ntdb, ltype, offset, 1,
						   flags);
			}
			if (ecode != NTDB_SUCCESS) {
				return ecode;
			}
		}
	}

	ntdb->file->lockrecs[ntdb->file->num_lockrecs].owner = ntdb;
	ntdb->file->lockrecs[ntdb->file->num_lockrecs].off = offset;
	ntdb->file->lockrecs[ntdb->file->num_lockrecs].count = 1;
	ntdb->file->lockrecs[ntdb->file->num_lockrecs].ltype = ltype;
	ntdb->file->num_lockrecs++;

	return NTDB_SUCCESS;
}

static enum NTDB_ERROR ntdb_nest_unlock(struct ntdb_context *ntdb,
				      ntdb_off_t off, int ltype)
{
	struct ntdb_lock *lck;
	enum NTDB_ERROR ecode;

	if (ntdb->flags & NTDB_NOLOCK)
		return NTDB_SUCCESS;

	lck = find_nestlock(ntdb, off, ntdb);
	if ((lck == NULL) || (lck->count == 0)) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_nest_unlock: no lock for %zu",
				  (size_t)off);
	}

	if (lck->count > 1) {
		lck->count--;
		return NTDB_SUCCESS;
	}

	/*
	 * This lock has count==1 left, so we need to unlock it in the
	 * kernel. We don't bother with decrementing the in-memory array
	 * element, we're about to overwrite it with the last array element
	 * anyway.
	 */
	ecode = ntdb_brunlock(ntdb, ltype, off, 1);

	/*
	 * Shrink the array by overwriting the element just unlocked with the
	 * last array element.
	 */
	*lck = ntdb->file->lockrecs[--ntdb->file->num_lockrecs];

	return ecode;
}

/*
  get the transaction lock
 */
enum NTDB_ERROR ntdb_transaction_lock(struct ntdb_context *ntdb, int ltype)
{
	return ntdb_nest_lock(ntdb, NTDB_TRANSACTION_LOCK, ltype, NTDB_LOCK_WAIT);
}

/*
  release the transaction lock
 */
void ntdb_transaction_unlock(struct ntdb_context *ntdb, int ltype)
{
	ntdb_nest_unlock(ntdb, NTDB_TRANSACTION_LOCK, ltype);
}

/* We only need to lock individual bytes, but Linux merges consecutive locks
 * so we lock in contiguous ranges. */
static enum NTDB_ERROR ntdb_lock_gradual(struct ntdb_context *ntdb,
				       int ltype, enum ntdb_lock_flags flags,
				       ntdb_off_t off, ntdb_off_t len)
{
	enum NTDB_ERROR ecode;
	enum ntdb_lock_flags nb_flags = (flags & ~NTDB_LOCK_WAIT);

	if (len <= 1) {
		/* 0 would mean to end-of-file... */
		assert(len != 0);
		/* Single hash.  Just do blocking lock. */
		return ntdb_brlock(ntdb, ltype, off, len, flags);
	}

	/* First we try non-blocking. */
	ecode = ntdb_brlock(ntdb, ltype, off, len, nb_flags);
	if (ecode != NTDB_ERR_LOCK) {
		return ecode;
	}

	/* Try locking first half, then second. */
	ecode = ntdb_lock_gradual(ntdb, ltype, flags, off, len / 2);
	if (ecode != NTDB_SUCCESS)
		return ecode;

	ecode = ntdb_lock_gradual(ntdb, ltype, flags,
				 off + len / 2, len - len / 2);
	if (ecode != NTDB_SUCCESS) {
		ntdb_brunlock(ntdb, ltype, off, len / 2);
	}
	return ecode;
}

/* lock/unlock entire database.  It can only be upgradable if you have some
 * other way of guaranteeing exclusivity (ie. transaction write lock). */
enum NTDB_ERROR ntdb_allrecord_lock(struct ntdb_context *ntdb, int ltype,
				  enum ntdb_lock_flags flags, bool upgradable)
{
	enum NTDB_ERROR ecode;
	ntdb_bool_err berr;

	if (ntdb->flags & NTDB_NOLOCK) {
		return NTDB_SUCCESS;
	}

	if (!check_lock_pid(ntdb, "ntdb_allrecord_lock", true)) {
		return NTDB_ERR_LOCK;
	}

	if (ntdb->file->allrecord_lock.count) {
		if (ntdb->file->allrecord_lock.owner != ntdb) {
			return owner_conflict(ntdb, "ntdb_allrecord_lock");
		}

		if (ltype == F_RDLCK
		    || ntdb->file->allrecord_lock.ltype == F_WRLCK) {
			ntdb->file->allrecord_lock.count++;
			return NTDB_SUCCESS;
		}

		/* a global lock of a different type exists */
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
				  "ntdb_allrecord_lock: already have %s lock",
				  ntdb->file->allrecord_lock.ltype == F_RDLCK
				  ? "read" : "write");
	}

	if (ntdb_has_hash_locks(ntdb)) {
		/* can't combine global and chain locks */
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
				  "ntdb_allrecord_lock:"
				  " already have chain lock");
	}

	if (upgradable && ltype != F_RDLCK) {
		/* ntdb error: you can't upgrade a write lock! */
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_allrecord_lock:"
				  " can't upgrade a write lock");
	}

	ntdb->stats.locks++;
again:
	/* Lock hashes, gradually. */
	ecode = ntdb_lock_gradual(ntdb, ltype, flags, NTDB_HASH_LOCK_START,
				  1 << ntdb->hash_bits);
	if (ecode != NTDB_SUCCESS)
		return ecode;

	/* Lock free tables: there to end of file. */
	ecode = ntdb_brlock(ntdb, ltype,
			    NTDB_HASH_LOCK_START + (1 << ntdb->hash_bits),
			    0, flags);
	if (ecode != NTDB_SUCCESS) {
		ntdb_brunlock(ntdb, ltype, NTDB_HASH_LOCK_START,
			      1 << ntdb->hash_bits);
		return ecode;
	}

	ntdb->file->allrecord_lock.owner = ntdb;
	ntdb->file->allrecord_lock.count = 1;
	/* If it's upgradable, it's actually exclusive so we can treat
	 * it as a write lock. */
	ntdb->file->allrecord_lock.ltype = upgradable ? F_WRLCK : ltype;
	ntdb->file->allrecord_lock.off = upgradable;

	/* Now check for needing recovery. */
	if (flags & NTDB_LOCK_NOCHECK)
		return NTDB_SUCCESS;

	berr = ntdb_needs_recovery(ntdb);
	if (likely(berr == false))
		return NTDB_SUCCESS;

	ntdb_allrecord_unlock(ntdb, ltype);
	if (berr < 0)
		return NTDB_OFF_TO_ERR(berr);
	ecode = ntdb_lock_and_recover(ntdb);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}
	goto again;
}

enum NTDB_ERROR ntdb_lock_open(struct ntdb_context *ntdb,
			     int ltype, enum ntdb_lock_flags flags)
{
	return ntdb_nest_lock(ntdb, NTDB_OPEN_LOCK, ltype, flags);
}

void ntdb_unlock_open(struct ntdb_context *ntdb, int ltype)
{
	ntdb_nest_unlock(ntdb, NTDB_OPEN_LOCK, ltype);
}

bool ntdb_has_open_lock(struct ntdb_context *ntdb)
{
	return !(ntdb->flags & NTDB_NOLOCK)
		&& find_nestlock(ntdb, NTDB_OPEN_LOCK, ntdb) != NULL;
}

enum NTDB_ERROR ntdb_lock_expand(struct ntdb_context *ntdb, int ltype)
{
	/* Lock doesn't protect data, so don't check (we recurse if we do!) */
	return ntdb_nest_lock(ntdb, NTDB_EXPANSION_LOCK, ltype,
			     NTDB_LOCK_WAIT | NTDB_LOCK_NOCHECK);
}

void ntdb_unlock_expand(struct ntdb_context *ntdb, int ltype)
{
	ntdb_nest_unlock(ntdb, NTDB_EXPANSION_LOCK, ltype);
}

/* unlock entire db */
void ntdb_allrecord_unlock(struct ntdb_context *ntdb, int ltype)
{
	if (ntdb->flags & NTDB_NOLOCK)
		return;

	if (ntdb->file->allrecord_lock.count == 0) {
		ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
			   "ntdb_allrecord_unlock: not locked!");
		return;
	}

	if (ntdb->file->allrecord_lock.owner != ntdb) {
		ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
			   "ntdb_allrecord_unlock: not locked by us!");
		return;
	}

	/* Upgradable locks are marked as write locks. */
	if (ntdb->file->allrecord_lock.ltype != ltype
	    && (!ntdb->file->allrecord_lock.off || ltype != F_RDLCK)) {
		ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
			   "ntdb_allrecord_unlock: have %s lock",
			   ntdb->file->allrecord_lock.ltype == F_RDLCK
			   ? "read" : "write");
		return;
	}

	if (ntdb->file->allrecord_lock.count > 1) {
		ntdb->file->allrecord_lock.count--;
		return;
	}

	ntdb->file->allrecord_lock.count = 0;
	ntdb->file->allrecord_lock.ltype = 0;

	ntdb_brunlock(ntdb, ltype, NTDB_HASH_LOCK_START, 0);
}

bool ntdb_has_expansion_lock(struct ntdb_context *ntdb)
{
	return find_nestlock(ntdb, NTDB_EXPANSION_LOCK, ntdb) != NULL;
}

bool ntdb_has_hash_locks(struct ntdb_context *ntdb)
{
	unsigned int i;

	for (i=0; i<ntdb->file->num_lockrecs; i++) {
		if (ntdb->file->lockrecs[i].off >= NTDB_HASH_LOCK_START
		    && ntdb->file->lockrecs[i].off < (NTDB_HASH_LOCK_START
						      + (1 << ntdb->hash_bits)))
			return true;
	}
	return false;
}

static bool ntdb_has_free_lock(struct ntdb_context *ntdb)
{
	unsigned int i;

	if (ntdb->flags & NTDB_NOLOCK)
		return false;

	for (i=0; i<ntdb->file->num_lockrecs; i++) {
		if (ntdb->file->lockrecs[i].off
		    > NTDB_HASH_LOCK_START + (1 << ntdb->hash_bits))
			return true;
	}
	return false;
}

enum NTDB_ERROR ntdb_lock_hash(struct ntdb_context *ntdb,
			       unsigned int h,
			       int ltype)
{
	unsigned l = NTDB_HASH_LOCK_START + h;

	assert(h < (1 << ntdb->hash_bits));

	/* a allrecord lock allows us to avoid per chain locks */
	if (ntdb->file->allrecord_lock.count) {
		if (!check_lock_pid(ntdb, "ntdb_lock_hashes", true))
			return NTDB_ERR_LOCK;

		if (ntdb->file->allrecord_lock.owner != ntdb)
			return owner_conflict(ntdb, "ntdb_lock_hashes");
		if (ltype == ntdb->file->allrecord_lock.ltype
		    || ltype == F_RDLCK) {
			return NTDB_SUCCESS;
		}

		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
				  "ntdb_lock_hashes:"
				  " already have %s allrecordlock",
				  ntdb->file->allrecord_lock.ltype == F_RDLCK
				  ? "read" : "write");
	}

	if (ntdb_has_free_lock(ntdb)) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_lock_hashes: already have free lock");
	}

	if (ntdb_has_expansion_lock(ntdb)) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_lock_hashes:"
				  " already have expansion lock");
	}

	return ntdb_nest_lock(ntdb, l, ltype, NTDB_LOCK_WAIT);
}

enum NTDB_ERROR ntdb_unlock_hash(struct ntdb_context *ntdb,
				 unsigned int h, int ltype)
{
	unsigned l = NTDB_HASH_LOCK_START + (h & ((1 << ntdb->hash_bits)-1));

	if (ntdb->flags & NTDB_NOLOCK)
		return 0;

	/* a allrecord lock allows us to avoid per chain locks */
	if (ntdb->file->allrecord_lock.count) {
		if (ntdb->file->allrecord_lock.ltype == F_RDLCK
		    && ltype == F_WRLCK) {
			return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
					  "ntdb_unlock_hashes RO allrecord!");
		}
		if (ntdb->file->allrecord_lock.owner != ntdb) {
			return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_USE_ERROR,
					  "ntdb_unlock_hashes:"
					  " not locked by us!");
		}
		return NTDB_SUCCESS;
	}

	return ntdb_nest_unlock(ntdb, l, ltype);
}

/* Hash locks use NTDB_HASH_LOCK_START + <number of hash entries>..
 * Then we begin; bucket offsets are sizeof(ntdb_len_t) apart, so we divide.
 * The result is that on 32 bit systems we don't use lock values > 2^31 on
 * files that are less than 4GB.
 */
static ntdb_off_t free_lock_off(const struct ntdb_context *ntdb,
				ntdb_off_t b_off)
{
	return NTDB_HASH_LOCK_START + (1 << ntdb->hash_bits)
		+ b_off / sizeof(ntdb_off_t);
}

enum NTDB_ERROR ntdb_lock_free_bucket(struct ntdb_context *ntdb, ntdb_off_t b_off,
				    enum ntdb_lock_flags waitflag)
{
	assert(b_off >= sizeof(struct ntdb_header));

	if (ntdb->flags & NTDB_NOLOCK)
		return 0;

	/* a allrecord lock allows us to avoid per chain locks */
	if (ntdb->file->allrecord_lock.count) {
		if (!check_lock_pid(ntdb, "ntdb_lock_free_bucket", true))
			return NTDB_ERR_LOCK;

		if (ntdb->file->allrecord_lock.owner != ntdb) {
			return owner_conflict(ntdb, "ntdb_lock_free_bucket");
		}

		if (ntdb->file->allrecord_lock.ltype == F_WRLCK)
			return 0;
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_lock_free_bucket with"
				  " read-only allrecordlock!");
	}

#if 0 /* FIXME */
	if (ntdb_has_expansion_lock(ntdb)) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_lock_free_bucket:"
				  " already have expansion lock");
	}
#endif

	return ntdb_nest_lock(ntdb, free_lock_off(ntdb, b_off), F_WRLCK,
			      waitflag);
}

void ntdb_unlock_free_bucket(struct ntdb_context *ntdb, ntdb_off_t b_off)
{
	if (ntdb->file->allrecord_lock.count)
		return;

	ntdb_nest_unlock(ntdb, free_lock_off(ntdb, b_off), F_WRLCK);
}

_PUBLIC_ enum NTDB_ERROR ntdb_lockall(struct ntdb_context *ntdb)
{
	return ntdb_allrecord_lock(ntdb, F_WRLCK, NTDB_LOCK_WAIT, false);
}

_PUBLIC_ void ntdb_unlockall(struct ntdb_context *ntdb)
{
	ntdb_allrecord_unlock(ntdb, F_WRLCK);
}

_PUBLIC_ enum NTDB_ERROR ntdb_lockall_read(struct ntdb_context *ntdb)
{
	return ntdb_allrecord_lock(ntdb, F_RDLCK, NTDB_LOCK_WAIT, false);
}

_PUBLIC_ void ntdb_unlockall_read(struct ntdb_context *ntdb)
{
	ntdb_allrecord_unlock(ntdb, F_RDLCK);
}

void ntdb_lock_cleanup(struct ntdb_context *ntdb)
{
	unsigned int i;

	/* We don't want to warn: they're allowed to close ntdb after fork. */
	if (!check_lock_pid(ntdb, "ntdb_close", false))
		return;

	while (ntdb->file->allrecord_lock.count
	       && ntdb->file->allrecord_lock.owner == ntdb) {
		ntdb_allrecord_unlock(ntdb, ntdb->file->allrecord_lock.ltype);
	}

	for (i=0; i<ntdb->file->num_lockrecs; i++) {
		if (ntdb->file->lockrecs[i].owner == ntdb) {
			ntdb_nest_unlock(ntdb,
					ntdb->file->lockrecs[i].off,
					ntdb->file->lockrecs[i].ltype);
			i--;
		}
	}
}
