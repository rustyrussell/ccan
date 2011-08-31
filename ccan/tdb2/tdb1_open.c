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

/* all contexts, to ensure no double-opens (fcntl locks don't nest!) */
static struct tdb1_context *tdb1s = NULL;

/* We use two hashes to double-check they're using the right hash function. */
void tdb1_header_hash(struct tdb1_context *tdb,
		     uint32_t *magic1_hash, uint32_t *magic2_hash)
{
	TDB_DATA hash_key;
	uint32_t tdb1_magic = TDB1_MAGIC;

	hash_key.dptr = (unsigned char *)TDB_MAGIC_FOOD;
	hash_key.dsize = sizeof(TDB_MAGIC_FOOD);
	*magic1_hash = tdb->hash_fn(&hash_key);

	hash_key.dptr = (unsigned char *)TDB1_CONV(tdb1_magic);
	hash_key.dsize = sizeof(tdb1_magic);
	*magic2_hash = tdb->hash_fn(&hash_key);

	/* Make sure at least one hash is non-zero! */
	if (*magic1_hash == 0 && *magic2_hash == 0)
		*magic1_hash = 1;
}

/* initialise a new database with a specified hash size */
static int tdb1_new_database(struct tdb1_context *tdb, int hash_size)
{
	struct tdb1_header *newdb;
	size_t size;
	int ret = -1;

	/* We make it up in memory, then write it out if not internal */
	size = sizeof(struct tdb1_header) + (hash_size+1)*sizeof(tdb1_off_t);
	if (!(newdb = (struct tdb1_header *)calloc(size, 1))) {
		tdb->last_error = TDB_ERR_OOM;
		return -1;
	}

	/* Fill in the header */
	newdb->version = TDB1_VERSION;
	newdb->hash_size = hash_size;

	tdb1_header_hash(tdb, &newdb->magic1_hash, &newdb->magic2_hash);

	/* Make sure older tdbs (which don't check the magic hash fields)
	 * will refuse to open this TDB. */
	if (tdb->hash_fn == tdb1_incompatible_hash)
		newdb->rwlocks = TDB1_HASH_RWLOCK_MAGIC;

	if (tdb->flags & TDB1_INTERNAL) {
		tdb->map_size = size;
		tdb->map_ptr = (char *)newdb;
		memcpy(&tdb->header, newdb, sizeof(tdb->header));
		/* Convert the `ondisk' version if asked. */
		TDB1_CONV(*newdb);
		return 0;
	}
	if (lseek(tdb->fd, 0, SEEK_SET) == -1)
		goto fail;

	if (ftruncate(tdb->fd, 0) == -1)
		goto fail;

	/* This creates an endian-converted header, as if read from disk */
	TDB1_CONV(*newdb);
	memcpy(&tdb->header, newdb, sizeof(tdb->header));
	/* Don't endian-convert the magic food! */
	memcpy(newdb->magic_food, TDB_MAGIC_FOOD, strlen(TDB_MAGIC_FOOD)+1);
	/* we still have "ret == -1" here */
	if (tdb1_write_all(tdb->fd, newdb, size))
		ret = 0;

  fail:
	SAFE_FREE(newdb);
	return ret;
}



static int tdb1_already_open(dev_t device,
			    ino_t ino)
{
	struct tdb1_context *i;

	for (i = tdb1s; i; i = i->next) {
		if (i->device == device && i->inode == ino) {
			return 1;
		}
	}

	return 0;
}

/* open the database, creating it if necessary

   The open_flags and mode are passed straight to the open call on the
   database file. A flags value of O_WRONLY is invalid. The hash size
   is advisory, use zero for a default value.

   Return is NULL on error, in which case errno is also set.  Don't
   try to call tdb1_error or tdb1_errname, just do strerror(errno).

   @param name may be NULL for internal databases. */
struct tdb1_context *tdb1_open(const char *name, int hash_size, int tdb1_flags,
		      int open_flags, mode_t mode)
{
	return tdb1_open_ex(name, hash_size, tdb1_flags, open_flags, mode, NULL, NULL);
}

static bool hash_correct(struct tdb1_context *tdb,
			 uint32_t *m1, uint32_t *m2)
{
	tdb1_header_hash(tdb, m1, m2);
	return (tdb->header.magic1_hash == *m1 &&
		tdb->header.magic2_hash == *m2);
}

static bool check_header_hash(struct tdb1_context *tdb,
			      uint32_t *m1, uint32_t *m2)
{
	if (hash_correct(tdb, m1, m2))
		return true;

	/* If they use one inbuilt, try the other inbuilt hash. */
	if (tdb->hash_fn == tdb1_old_hash)
		tdb->hash_fn = tdb1_incompatible_hash;
	else if (tdb->hash_fn == tdb1_incompatible_hash)
		tdb->hash_fn = tdb1_old_hash;
	else
		return false;
	return hash_correct(tdb, m1, m2);
}

struct tdb1_context *tdb1_open_ex(const char *name, int hash_size, int tdb1_flags,
				int open_flags, mode_t mode,
				const struct tdb1_logging_context *log_ctx,
				tdb1_hash_func hash_fn)
{
	struct tdb1_context *tdb;
	struct stat st;
	int rev = 0, locked = 0;
	unsigned char *vp;
	uint32_t vertest;
	unsigned v;
	const char *hash_alg;
	uint32_t magic1, magic2;

	if (!(tdb = (struct tdb1_context *)calloc(1, sizeof *tdb))) {
		/* Can't log this */
		errno = ENOMEM;
		goto fail;
	}
	tdb1_io_init(tdb);
	tdb->fd = -1;
	tdb->name = NULL;
	tdb->map_ptr = NULL;
	tdb->flags = tdb1_flags;
	tdb->open_flags = open_flags;
	if (log_ctx) {
		tdb->log_fn = log_ctx->log_fn;
		tdb->log_data = log_ctx->log_private;
	} else
		tdb->log_fn = NULL;

	if (name == NULL && (tdb1_flags & TDB1_INTERNAL)) {
		name = "__TDB1_INTERNAL__";
	}

	if (name == NULL) {
		tdb->name = (char *)"__NULL__";
		tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
			   "tdb1_open_ex: called with name == NULL");
		tdb->name = NULL;
		errno = EINVAL;
		goto fail;
	}

	/* now make a copy of the name, as the caller memory might went away */
	if (!(tdb->name = (char *)strdup(name))) {
		/*
		 * set the name as the given string, so that tdb1_name() will
		 * work in case of an error.
		 */
		tdb->name = (char *)name;
		tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
			   "tdb1_open_ex: can't strdup(%s)", name);
		tdb->name = NULL;
		errno = ENOMEM;
		goto fail;
	}

	if (hash_fn) {
		tdb->hash_fn = hash_fn;
		if (hash_fn == tdb1_incompatible_hash)
			hash_alg = "tdb1_incompatible_hash";
		else
			hash_alg = "the user defined";
	} else {
		tdb->hash_fn = tdb1_old_hash;
		hash_alg = "default";
	}

	/* cache the page size */
	tdb->page_size = getpagesize();
	if (tdb->page_size <= 0) {
		tdb->page_size = 0x2000;
	}

	tdb->max_dead_records = (tdb1_flags & TDB1_VOLATILE) ? 5 : 0;

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
			   "tdb1_open_ex: can't open tdb %s write-only",
			   name);
		errno = EINVAL;
		goto fail;
	}

	if (hash_size == 0)
		hash_size = TDB1_DEFAULT_HASH_SIZE;
	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		tdb->read_only = 1;
		/* read only databases don't do locking or clear if first */
		tdb->flags |= TDB1_NOLOCK;
		tdb->flags &= ~TDB1_CLEAR_IF_FIRST;
	}

	if ((tdb->flags & TDB1_ALLOW_NESTING) &&
	    (tdb->flags & TDB1_DISALLOW_NESTING)) {
		tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
			   "tdb1_open_ex: "
			   "allow_nesting and disallow_nesting are not allowed together!");
		errno = EINVAL;
		goto fail;
	}

	/*
	 * TDB1_ALLOW_NESTING is the default behavior.
	 * Note: this may change in future versions!
	 */
	if (!(tdb->flags & TDB1_DISALLOW_NESTING)) {
		tdb->flags |= TDB1_ALLOW_NESTING;
	}

	/* internal databases don't mmap or lock, and start off cleared */
	if (tdb->flags & TDB1_INTERNAL) {
		tdb->flags |= (TDB1_NOLOCK | TDB1_NOMMAP);
		tdb->flags &= ~TDB1_CLEAR_IF_FIRST;
		if (tdb1_new_database(tdb, hash_size) != 0) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_open_ex: tdb1_new_database failed!");
			goto fail;
		}
		goto internal;
	}

	if ((tdb->fd = open(name, open_flags, mode)) == -1) {
		tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
			   "tdb1_open_ex: could not open file %s: %s",
			   name, strerror(errno));
		goto fail;	/* errno set by open(2) */
	}

	/* on exec, don't inherit the fd */
	v = fcntl(tdb->fd, F_GETFD, 0);
        fcntl(tdb->fd, F_SETFD, v | FD_CLOEXEC);

	/* ensure there is only one process initialising at once */
	if (tdb1_nest_lock(tdb, TDB1_OPEN_LOCK, F_WRLCK, TDB_LOCK_WAIT) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_open_ex: failed to get open lock on %s: %s",
			   name, strerror(errno));
		goto fail;	/* errno set by tdb1_brlock */
	}

	/* we need to zero database if we are the only one with it open */
	if ((tdb1_flags & TDB1_CLEAR_IF_FIRST) &&
	    (!tdb->read_only) &&
	    (locked = (tdb1_nest_lock(tdb, TDB1_ACTIVE_LOCK, F_WRLCK, TDB_LOCK_NOWAIT|TDB_LOCK_PROBE) == 0))) {
		open_flags |= O_CREAT;
		if (ftruncate(tdb->fd, 0) == -1) {
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb1_open_ex: "
				   "failed to truncate %s: %s",
				   name, strerror(errno));
			goto fail; /* errno set by ftruncate */
		}
	}

	errno = 0;
	if (read(tdb->fd, &tdb->header, sizeof(tdb->header)) != sizeof(tdb->header)
	    || strcmp(tdb->header.magic_food, TDB_MAGIC_FOOD) != 0) {
		if (!(open_flags & O_CREAT) || tdb1_new_database(tdb, hash_size) == -1) {
			if (errno == 0) {
				errno = EIO; /* ie bad format or something */
			}
			goto fail;
		}
		rev = (tdb->flags & TDB1_CONVERT);
	} else if (tdb->header.version != TDB1_VERSION
		   && !(rev = (tdb->header.version==TDB1_BYTEREV(TDB1_VERSION)))) {
		/* wrong version */
		errno = EIO;
		goto fail;
	}
	vp = (unsigned char *)&tdb->header.version;
	vertest = (((uint32_t)vp[0]) << 24) | (((uint32_t)vp[1]) << 16) |
		  (((uint32_t)vp[2]) << 8) | (uint32_t)vp[3];
	tdb->flags |= (vertest==TDB1_VERSION) ? TDB1_BIGENDIAN : 0;
	if (!rev)
		tdb->flags &= ~TDB1_CONVERT;
	else {
		tdb->flags |= TDB1_CONVERT;
		tdb1_convert(&tdb->header, sizeof(tdb->header));
	}
	if (fstat(tdb->fd, &st) == -1)
		goto fail;

	if (tdb->header.rwlocks != 0 &&
	    tdb->header.rwlocks != TDB1_HASH_RWLOCK_MAGIC) {
		tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
			   "tdb1_open_ex: spinlocks no longer supported");
		goto fail;
	}

	if ((tdb->header.magic1_hash == 0) && (tdb->header.magic2_hash == 0)) {
		/* older TDB without magic hash references */
		tdb->hash_fn = tdb1_old_hash;
	} else if (!check_header_hash(tdb, &magic1, &magic2)) {
		tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_USE_ERROR,
			   "tdb1_open_ex: "
			   "%s was not created with %s hash function we are using\n"
			   "magic1_hash[0x%08X %s 0x%08X] "
			   "magic2_hash[0x%08X %s 0x%08X]",
			   name, hash_alg,
			   tdb->header.magic1_hash,
			   (tdb->header.magic1_hash == magic1) ? "==" : "!=",
			   magic1,
			   tdb->header.magic2_hash,
			   (tdb->header.magic2_hash == magic2) ? "==" : "!=",
			   magic2);
		errno = EINVAL;
		goto fail;
	}

	/* Is it already in the open list?  If so, fail. */
	if (tdb1_already_open(st.st_dev, st.st_ino)) {
		tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_USE_ERROR,
			   "tdb1_open_ex: "
			   "%s (%d,%d) is already open in this process",
			   name, (int)st.st_dev, (int)st.st_ino);
		errno = EBUSY;
		goto fail;
	}

	tdb->map_size = st.st_size;
	tdb->device = st.st_dev;
	tdb->inode = st.st_ino;
	tdb1_mmap(tdb);
	if (locked) {
		if (tdb1_nest_unlock(tdb, TDB1_ACTIVE_LOCK, F_WRLCK) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_open_ex: "
				   "failed to release ACTIVE_LOCK on %s: %s",
				   name, strerror(errno));
			goto fail;
		}

	}

	/* We always need to do this if the CLEAR_IF_FIRST flag is set, even if
	   we didn't get the initial exclusive lock as we need to let all other
	   users know we're using it. */

	if (tdb1_flags & TDB1_CLEAR_IF_FIRST) {
		/* leave this lock in place to indicate it's in use */
		if (tdb1_nest_lock(tdb, TDB1_ACTIVE_LOCK, F_RDLCK, TDB_LOCK_WAIT) == -1) {
			goto fail;
		}
	}

	/* if needed, run recovery */
	if (tdb1_transaction_recover(tdb) == -1) {
		goto fail;
	}

 internal:
	/* Internal (memory-only) databases skip all the code above to
	 * do with disk files, and resume here by releasing their
	 * open lock and hooking into the active list. */
	if (tdb1_nest_unlock(tdb, TDB1_OPEN_LOCK, F_WRLCK) == -1) {
		goto fail;
	}
	tdb->next = tdb1s;
	tdb1s = tdb;
	return tdb;

 fail:
	{ int save_errno = errno;

	if (!tdb)
		return NULL;

	if (tdb->map_ptr) {
		if (tdb->flags & TDB1_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb1_munmap(tdb);
	}
	if (tdb->fd != -1)
		if (close(tdb->fd) != 0)
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb1_open_ex: failed to close tdb->fd on error!");
	SAFE_FREE(tdb->lockrecs);
	SAFE_FREE(tdb->name);
	SAFE_FREE(tdb);
	errno = save_errno;
	return NULL;
	}
}

/*
 * Set the maximum number of dead records per hash chain
 */

void tdb1_set_max_dead(struct tdb1_context *tdb, int max_dead)
{
	tdb->max_dead_records = max_dead;
}

/**
 * Close a database.
 *
 * @returns -1 for error; 0 for success.
 **/
int tdb1_close(struct tdb1_context *tdb)
{
	struct tdb1_context **i;
	int ret = 0;

	if (tdb->transaction) {
		tdb1_transaction_cancel(tdb);
	}

	if (tdb->map_ptr) {
		if (tdb->flags & TDB1_INTERNAL)
			SAFE_FREE(tdb->map_ptr);
		else
			tdb1_munmap(tdb);
	}
	SAFE_FREE(tdb->name);
	if (tdb->fd != -1) {
		ret = close(tdb->fd);
		tdb->fd = -1;
	}
	SAFE_FREE(tdb->lockrecs);

	/* Remove from contexts list */
	for (i = &tdb1s; *i; i = &(*i)->next) {
		if (*i == tdb) {
			*i = tdb->next;
			break;
		}
	}

	memset(tdb, 0, sizeof(*tdb));
	SAFE_FREE(tdb);

	return ret;
}
