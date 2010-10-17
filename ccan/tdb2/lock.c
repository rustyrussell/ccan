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

static int fcntl_lock(struct tdb_context *tdb,
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

static int fcntl_unlock(struct tdb_context *tdb, int rw, off_t off, off_t len)
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

/* a byte range locking function - return 0 on success
   this functions locks/unlocks 1 byte at the specified offset.

   note that a len of zero means lock to end of file
*/
static int tdb_brlock(struct tdb_context *tdb,
		      int rw_type, tdb_off_t offset, tdb_off_t len,
		      enum tdb_lock_flags flags)
{
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	if (rw_type == F_WRLCK && tdb->read_only) {
		tdb->ecode = TDB_ERR_RDONLY;
		return -1;
	}

	/* A 32 bit system cannot open a 64-bit file, but it could have
	 * expanded since then: check here. */
	if ((size_t)(offset + len) != offset + len) {
		tdb->ecode = TDB_ERR_IO;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_brlock: lock on giant offset %llu\n",
			 (long long)(offset + len));
		return -1;
	}

	do {
		ret = fcntl_lock(tdb, rw_type, offset, len,
				 flags & TDB_LOCK_WAIT);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		tdb->ecode = TDB_ERR_LOCK;
		/* Generic lock error. errno set by fcntl.
		 * EAGAIN is an expected return from non-blocking
		 * locks. */
		if (!(flags & TDB_LOCK_PROBE) && errno != EAGAIN) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_brlock failed (fd=%d) at"
				 " offset %llu rw_type=%d flags=%d len=%llu\n",
				 tdb->fd, (long long)offset, rw_type,
				 flags, (long long)len);
		}
		return -1;
	}
	return 0;
}

static int tdb_brunlock(struct tdb_context *tdb,
			int rw_type, tdb_off_t offset, size_t len)
{
	int ret;

	if (tdb->flags & TDB_NOLOCK) {
		return 0;
	}

	do {
		ret = fcntl_unlock(tdb, rw_type, offset, len);
	} while (ret == -1 && errno == EINTR);

	if (ret == -1) {
		tdb->log(tdb, TDB_DEBUG_TRACE, tdb->log_priv,
			 "tdb_brunlock failed (fd=%d) at offset %llu"
			 " rw_type=%d len=%llu\n",
			 tdb->fd, (long long)offset, rw_type, (long long)len);
	}
	return ret;
}

#if 0
/*
  upgrade a read lock to a write lock. This needs to be handled in a
  special way as some OSes (such as solaris) have too conservative
  deadlock detection and claim a deadlock when progress can be
  made. For those OSes we may loop for a while.  
*/
int tdb_allrecord_upgrade(struct tdb_context *tdb)
{
	int count = 1000;

	if (tdb->allrecord_lock.count != 1) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_upgrade failed: count %u too high\n",
			 tdb->allrecord_lock.count);
		return -1;
	}

	if (tdb->allrecord_lock.off != 1) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_upgrade failed: already upgraded?\n");
		return -1;
	}

	while (count--) {
		struct timeval tv;
		if (tdb_brlock(tdb, F_WRLCK,
			       TDB_HASH_LOCK_START
			       + (1ULL << tdb->header.v.hash_bits), 0,
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
	tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
		 "tdb_allrecord_upgrade failed\n");
	return -1;
}
#endif

static struct tdb_lock_type *find_nestlock(struct tdb_context *tdb,
					   tdb_off_t offset)
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
static int tdb_nest_lock(struct tdb_context *tdb, tdb_off_t offset, int ltype,
			 enum tdb_lock_flags flags)
{
	struct tdb_lock_type *new_lck;

	if (offset >= TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE + tdb->map_size / 8) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_nest_lock: invalid offset %llu ltype=%d\n",
			 (long long)offset, ltype);
		return -1;
	}

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	new_lck = find_nestlock(tdb, offset);
	if (new_lck) {
		/*
		 * Just increment the in-memory struct, posix locks
		 * don't stack.
		 */
		new_lck->count++;
		return 0;
	}

	new_lck = (struct tdb_lock_type *)realloc(
		tdb->lockrecs,
		sizeof(*tdb->lockrecs) * (tdb->num_lockrecs+1));
	if (new_lck == NULL) {
		tdb->ecode = TDB_ERR_OOM;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_nest_lock: unable to allocate %llu lock struct",
			 (long long)(tdb->num_lockrecs + 1));
		errno = ENOMEM;
		return -1;
	}
	tdb->lockrecs = new_lck;

	/* Since fcntl locks don't nest, we do a lock for the first one,
	   and simply bump the count for future ones */
	if (tdb_brlock(tdb, ltype, offset, 1, flags)) {
		return -1;
	}

	tdb->lockrecs[tdb->num_lockrecs].off = offset;
	tdb->lockrecs[tdb->num_lockrecs].count = 1;
	tdb->lockrecs[tdb->num_lockrecs].ltype = ltype;
	tdb->num_lockrecs++;

	return 0;
}

static int tdb_lock_and_recover(struct tdb_context *tdb)
{
#if 0 /* FIXME */

	int ret;

	/* We need to match locking order in transaction commit. */
	if (tdb_brlock(tdb, F_WRLCK, FREELIST_TOP, 0, TDB_LOCK_WAIT)) {
		return -1;
	}

	if (tdb_brlock(tdb, F_WRLCK, OPEN_LOCK, 1, TDB_LOCK_WAIT)) {
		tdb_brunlock(tdb, F_WRLCK, FREELIST_TOP, 0);
		return -1;
	}

	ret = tdb_transaction_recover(tdb);

	tdb_brunlock(tdb, F_WRLCK, OPEN_LOCK, 1);
	tdb_brunlock(tdb, F_WRLCK, FREELIST_TOP, 0);

	return ret;
#else
	abort();
	return -1;
#endif
}

static bool tdb_needs_recovery(struct tdb_context *tdb)
{
	/* FIXME */
	return false;
}

static int tdb_nest_unlock(struct tdb_context *tdb, tdb_off_t off, int ltype)
{
	int ret = -1;
	struct tdb_lock_type *lck;

	if (tdb->flags & TDB_NOLOCK)
		return 0;

	lck = find_nestlock(tdb, off);
	if ((lck == NULL) || (lck->count == 0)) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_nest_unlock: no lock for %llu\n", (long long)off);
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
	ret = tdb_brunlock(tdb, ltype, off, 1);

	/*
	 * Shrink the array by overwriting the element just unlocked with the
	 * last array element.
	 */
	*lck = tdb->lockrecs[--tdb->num_lockrecs];

	return ret;
}

#if 0
/*
  get the transaction lock
 */
int tdb_transaction_lock(struct tdb_context *tdb, int ltype,
			 enum tdb_lock_flags lockflags)
{
	return tdb_nest_lock(tdb, TRANSACTION_LOCK, ltype, lockflags);
}

/*
  release the transaction lock
 */
int tdb_transaction_unlock(struct tdb_context *tdb, int ltype)
{
	return tdb_nest_unlock(tdb, TRANSACTION_LOCK, ltype, false);
}
#endif

/* We only need to lock individual bytes, but Linux merges consecutive locks
 * so we lock in contiguous ranges. */
static int tdb_lock_gradual(struct tdb_context *tdb,
			    int ltype, enum tdb_lock_flags flags,
			    tdb_off_t off, tdb_off_t len)
{
	int ret;
	enum tdb_lock_flags nb_flags = (flags & ~TDB_LOCK_WAIT);

	if (len <= 1) {
		/* 0 would mean to end-of-file... */
		assert(len != 0);
		/* Single hash.  Just do blocking lock. */
		return tdb_brlock(tdb, ltype, off, len, flags);
	}

	/* First we try non-blocking. */
	ret = tdb_brlock(tdb, ltype, off, len, nb_flags);
	if (ret == 0) {
		return 0;
	}

	/* Try locking first half, then second. */
	ret = tdb_lock_gradual(tdb, ltype, flags, off, len / 2);
	if (ret == -1)
		return -1;

	ret = tdb_lock_gradual(tdb, ltype, flags,
				    off + len / 2, len - len / 2);
	if (ret == -1) {
		tdb_brunlock(tdb, ltype, off, len / 2);
		return -1;
	}
	return 0;
}

/* lock/unlock entire database.  It can only be upgradable if you have some
 * other way of guaranteeing exclusivity (ie. transaction write lock). */
int tdb_allrecord_lock(struct tdb_context *tdb, int ltype,
		       enum tdb_lock_flags flags, bool upgradable)
{
	/* FIXME: There are no locks on read-only dbs */
	if (tdb->read_only) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_lock: read-only\n");
		return -1;
	}

	if (tdb->allrecord_lock.count && tdb->allrecord_lock.ltype == ltype) {
		tdb->allrecord_lock.count++;
		return 0;
	}

	if (tdb->allrecord_lock.count) {
		/* a global lock of a different type exists */
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_lock: already have %s lock\n",
			 tdb->allrecord_lock.ltype == F_RDLCK
			 ? "read" : "write");
		return -1;
	}

	if (tdb_has_locks(tdb)) {
		/* can't combine global and chain locks */
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_lock: already have chain lock\n");
		return -1;
	}

	if (upgradable && ltype != F_RDLCK) {
		/* tdb error: you can't upgrade a write lock! */
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_lock: can't upgrade a write lock\n");
		return -1;
	}

again:
	/* Lock hashes, gradually. */
	if (tdb_lock_gradual(tdb, ltype, flags, TDB_HASH_LOCK_START,
			     TDB_HASH_LOCK_RANGE)) {
		if (!(flags & TDB_LOCK_PROBE)) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_allrecord_lock hashes failed (%s)\n",
				 strerror(errno));
		}
		return -1;
	}

	/* Lock free lists: there to end of file. */
	if (tdb_brlock(tdb, ltype, TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE,
		       0, flags)) {
		if (!(flags & TDB_LOCK_PROBE)) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_allrecord_lock freelist failed (%s)\n",
				 strerror(errno));
		}
		tdb_brunlock(tdb, ltype, TDB_HASH_LOCK_START, 
			     TDB_HASH_LOCK_RANGE);
		return -1;
	}

	tdb->allrecord_lock.count = 1;
	/* If it's upgradable, it's actually exclusive so we can treat
	 * it as a write lock. */
	tdb->allrecord_lock.ltype = upgradable ? F_WRLCK : ltype;
	tdb->allrecord_lock.off = upgradable;

	/* Now check for needing recovery. */
	if (unlikely(tdb_needs_recovery(tdb))) {
		tdb_allrecord_unlock(tdb, ltype);
		if (tdb_lock_and_recover(tdb) == -1) {
			return -1;
		}		
		goto again;
	}

	return 0;
}

int tdb_lock_open(struct tdb_context *tdb)
{
	return tdb_nest_lock(tdb, TDB_OPEN_LOCK, F_WRLCK, TDB_LOCK_WAIT);
}

void tdb_unlock_open(struct tdb_context *tdb)
{
	tdb_nest_unlock(tdb, TDB_OPEN_LOCK, F_WRLCK);
}

int tdb_lock_expand(struct tdb_context *tdb, int ltype)
{
	return tdb_nest_lock(tdb, TDB_EXPANSION_LOCK, ltype, TDB_LOCK_WAIT);
}

void tdb_unlock_expand(struct tdb_context *tdb, int ltype)
{
	tdb_nest_unlock(tdb, TDB_EXPANSION_LOCK, ltype);
}

/* unlock entire db */
int tdb_allrecord_unlock(struct tdb_context *tdb, int ltype)
{
	/* FIXME: There are no locks on read-only dbs */
	if (tdb->read_only) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_unlock: read-only\n");
		return -1;
	}

	if (tdb->allrecord_lock.count == 0) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_unlock: not locked!\n");
		return -1;
	}

	/* Upgradable locks are marked as write locks. */
	if (tdb->allrecord_lock.ltype != ltype
	    && (!tdb->allrecord_lock.off || ltype != F_RDLCK)) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_allrecord_unlock: have %s lock\n",
			 tdb->allrecord_lock.ltype == F_RDLCK
			 ? "read" : "write");
		return -1;
	}

	if (tdb->allrecord_lock.count > 1) {
		tdb->allrecord_lock.count--;
		return 0;
	}

	tdb->allrecord_lock.count = 0;
	tdb->allrecord_lock.ltype = 0;

	return tdb_brunlock(tdb, ltype, TDB_HASH_LOCK_START, 0);
}

bool tdb_has_expansion_lock(struct tdb_context *tdb)
{
	return find_nestlock(tdb, TDB_EXPANSION_LOCK) != NULL;
}

bool tdb_has_locks(struct tdb_context *tdb)
{
	return tdb->allrecord_lock.count || tdb->num_lockrecs;
}

#if 0
/* lock entire database with write lock */
int tdb_lockall(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall");
	return tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false);
}

/* lock entire database with write lock - nonblocking varient */
int tdb_lockall_nonblock(struct tdb_context *tdb)
{
	int ret = tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_NOWAIT, false);
	tdb_trace_ret(tdb, "tdb_lockall_nonblock", ret);
	return ret;
}

/* unlock entire database with write lock */
int tdb_unlockall(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_unlockall");
	return tdb_allrecord_unlock(tdb, F_WRLCK);
}

/* lock entire database with read lock */
int tdb_lockall_read(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_lockall_read");
	return tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false);
}

/* lock entire database with read lock - nonblock varient */
int tdb_lockall_read_nonblock(struct tdb_context *tdb)
{
	int ret = tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_NOWAIT, false);
	tdb_trace_ret(tdb, "tdb_lockall_read_nonblock", ret);
	return ret;
}

/* unlock entire database with read lock */
int tdb_unlockall_read(struct tdb_context *tdb)
{
	tdb_trace(tdb, "tdb_unlockall_read");
	return tdb_allrecord_unlock(tdb, F_RDLCK);
}
#endif

static bool tdb_has_free_lock(struct tdb_context *tdb)
{
	unsigned int i;

	for (i=0; i<tdb->num_lockrecs; i++) {
		if (tdb->lockrecs[i].off
		    > TDB_HASH_LOCK_START + TDB_HASH_LOCK_RANGE)
			return true;
	}
	return false;
}

int tdb_lock_hashes(struct tdb_context *tdb,
		    tdb_off_t hash_lock,
		    tdb_len_t hash_range,
		    int ltype, enum tdb_lock_flags waitflag)
{
	/* FIXME: Do this properly, using hlock_range */
	unsigned lock = TDB_HASH_LOCK_START
		+ (hash_lock >> (64 - TDB_HASH_LOCK_RANGE_BITS));

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->allrecord_lock.count &&
	    (ltype == tdb->allrecord_lock.ltype || ltype == F_RDLCK)) {
		return 0;
	}

	if (tdb->allrecord_lock.count) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_lock_hashes: have %s allrecordlock\n",
			 tdb->allrecord_lock.ltype == F_RDLCK
			 ? "read" : "write");
		return -1;
	}

	if (tdb_has_free_lock(tdb)) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_lock_hashes: have free lock already\n");
		return -1;
	}

	if (tdb_has_expansion_lock(tdb)) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_lock_hashes: have expansion lock already\n");
		return -1;
	}

	return tdb_nest_lock(tdb, lock, ltype, waitflag);
}

int tdb_unlock_hashes(struct tdb_context *tdb,
		      tdb_off_t hash_lock,
		      tdb_len_t hash_range, int ltype)
{
	unsigned lock = TDB_HASH_LOCK_START
		+ (hash_lock >> (64 - TDB_HASH_LOCK_RANGE_BITS));

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->allrecord_lock.count) {
		if (tdb->allrecord_lock.ltype == F_RDLCK
		    && ltype == F_WRLCK) {
			tdb->ecode = TDB_ERR_LOCK;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "tdb_unlock_hashes RO allrecord!\n");
			return -1;
		}
		return 0;
	}

	return tdb_nest_unlock(tdb, lock, ltype);
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

int tdb_lock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off,
			 enum tdb_lock_flags waitflag)
{
	assert(b_off >= sizeof(struct tdb_header));

	/* a allrecord lock allows us to avoid per chain locks */
	if (tdb->allrecord_lock.count) {
		if (tdb->allrecord_lock.ltype == F_WRLCK)
			return 0;
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_lock_free_bucket with RO allrecordlock!\n");
		return -1;
	}

#if 0 /* FIXME */
	if (tdb_has_expansion_lock(tdb)) {
		tdb->ecode = TDB_ERR_LOCK;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_lock_free_bucket: have expansion lock already\n");
		return -1;
	}
#endif

	return tdb_nest_lock(tdb, free_lock_off(b_off), F_WRLCK, waitflag);
}

void tdb_unlock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off)
{
	if (tdb->allrecord_lock.count)
		return;

	tdb_nest_unlock(tdb, free_lock_off(b_off), F_WRLCK);
}

/* Even if the entry isn't in this hash bucket, you'd have to lock this
 * bucket to find it. */
static int chainlock(struct tdb_context *tdb, const TDB_DATA *key,
		     int ltype, enum tdb_lock_flags waitflag,
		     const char *func)
{
	int ret;
	uint64_t h = tdb_hash(tdb, key->dptr, key->dsize);

	ret = tdb_lock_hashes(tdb, h, 1, ltype, waitflag);
	tdb_trace_1rec(tdb, func, *key);
	return ret;
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
int tdb_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	return chainlock(tdb, &key, F_WRLCK, TDB_LOCK_WAIT, "tdb_chainlock");
}

int tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	uint64_t h = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_trace_1rec(tdb, "tdb_chainunlock", key);
	return tdb_unlock_hashes(tdb, h, 1, F_WRLCK);
}

#if 0
/* lock/unlock one hash chain, non-blocking. This is meant to be used
   to reduce contention - it cannot guarantee how many records will be
   locked */
int tdb_chainlock_nonblock(struct tdb_context *tdb, TDB_DATA key)
{
	return chainlock(tdb, &key, F_WRLCK, TDB_LOCK_NOWAIT,
			 "tdb_chainlock_nonblock");
}

int tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return chainlock(tdb, &key, F_RDLCK, TDB_LOCK_WAIT,
			 "tdb_chainlock_read");
}

int tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	uint64_t h = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_trace_1rec(tdb, "tdb_chainunlock_read", key);
	return tdb_unlock_list(tdb, h & ((1ULL << tdb->header.v.hash_bits)-1),
			       F_RDLCK);
}

/* record lock stops delete underneath */
int tdb_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	if (tdb->allrecord_lock.count) {
		return 0;
	}
	return off ? tdb_brlock(tdb, F_RDLCK, off, 1, TDB_LOCK_WAIT) : 0;
}

/*
  Write locks override our own fcntl readlocks, so check it here.
  Note this is meant to be F_SETLK, *not* F_SETLKW, as it's not
  an error to fail to get the lock here.
*/
int tdb_write_lock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			return -1;
	if (tdb->allrecord_lock.count) {
		if (tdb->allrecord_lock.ltype == F_WRLCK) {
			return 0;
		}
		return -1;
	}
	return tdb_brlock(tdb, F_WRLCK, off, 1, TDB_LOCK_NOWAIT|TDB_LOCK_PROBE);
}

int tdb_write_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	if (tdb->allrecord_lock.count) {
		return 0;
	}
	return tdb_brunlock(tdb, F_WRLCK, off, 1);
}

/* fcntl locks don't stack: avoid unlocking someone else's */
int tdb_unlock_record(struct tdb_context *tdb, tdb_off_t off)
{
	struct tdb_traverse_lock *i;
	uint32_t count = 0;

	if (tdb->allrecord_lock.count) {
		return 0;
	}

	if (off == 0)
		return 0;
	for (i = &tdb->travlocks; i; i = i->next)
		if (i->off == off)
			count++;
	return (count == 1 ? tdb_brunlock(tdb, F_RDLCK, off, 1) : 0);
}

/* The transaction code uses this to remove all locks. */
void tdb_release_transaction_locks(struct tdb_context *tdb)
{
	unsigned int i;

	if (tdb->allrecord_lock.count != 0) {
		tdb_off_t hash_size, free_size;

		hash_size = (1ULL << tdb->header.v.hash_bits)
			* sizeof(tdb_off_t);
		free_size = tdb->header.v.free_zones 
			* (tdb->header.v.free_buckets + 1) * sizeof(tdb_off_t);

		tdb_brunlock(tdb, tdb->allrecord_lock.ltype,
			     tdb->header.v.hash_off, hash_size);
		tdb_brunlock(tdb, tdb->allrecord_lock.ltype,
			     tdb->header.v.free_off, free_size);
		tdb->allrecord_lock.count = 0;
		tdb->allrecord_lock.ltype = 0;
	}

	for (i = 0; i<tdb->num_lockrecs; i++) {
		struct tdb_lock_type *lck = &tdb->lockrecs[i];

		tdb_brunlock(tdb, lck->ltype, lck->off, 1);
	}
	tdb->num_lockrecs = 0;
	SAFE_FREE(tdb->lockrecs);
	tdb->header_uptodate = false;
}
#endif

void tdb_lock_init(struct tdb_context *tdb)
{
	tdb->num_lockrecs = 0;
	tdb->lockrecs = NULL;
	tdb->allrecord_lock.count = 0;
}
