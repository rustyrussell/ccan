 /*
   Trivial Database 2: opening and closing TDBs
   Copyright (C) Rusty Russell 2010

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
#include <assert.h>

/* all tdbs, to detect double-opens (fcntl file don't nest!) */
static struct tdb_context *tdbs = NULL;

static struct tdb_file *find_file(dev_t device, ino_t ino)
{
	struct tdb_context *i;

	for (i = tdbs; i; i = i->next) {
		if (i->file->device == device && i->file->inode == ino) {
			i->file->refcnt++;
			return i->file;
		}
	}
	return NULL;
}

static bool read_all(int fd, void *buf, size_t len)
{
	while (len) {
		ssize_t ret;
		ret = read(fd, buf, len);
		if (ret < 0)
			return false;
		if (ret == 0) {
			/* ETOOSHORT? */
			errno = EWOULDBLOCK;
			return false;
		}
		buf = (char *)buf + ret;
		len -= ret;
	}
	return true;
}

static uint64_t random_number(struct tdb_context *tdb)
{
	int fd;
	uint64_t ret = 0;
	struct timeval now;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		if (read_all(fd, &ret, sizeof(ret))) {
			close(fd);
			return ret;
		}
		close(fd);
	}
	/* FIXME: Untested!  Based on Wikipedia protocol description! */
	fd = open("/dev/egd-pool", O_RDWR);
	if (fd >= 0) {
		/* Command is 1, next byte is size we want to read. */
		char cmd[2] = { 1, sizeof(uint64_t) };
		if (write(fd, cmd, sizeof(cmd)) == sizeof(cmd)) {
			char reply[1 + sizeof(uint64_t)];
			int r = read(fd, reply, sizeof(reply));
			if (r > 1) {
				/* Copy at least some bytes. */
				memcpy(&ret, reply+1, r - 1);
				if (reply[0] == sizeof(uint64_t)
				    && r == sizeof(reply)) {
					close(fd);
					return ret;
				}
			}
		}
		close(fd);
	}

	/* Fallback: pid and time. */
	gettimeofday(&now, NULL);
	ret = getpid() * 100132289ULL + now.tv_sec * 1000000ULL + now.tv_usec;
	tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
		   "tdb_open: random from getpid and time");
	return ret;
}

static void tdb2_context_init(struct tdb_context *tdb)
{
	/* Initialize the TDB2 fields here */
	tdb_io_init(tdb);
	tdb->tdb2.direct_access = 0;
	tdb->tdb2.transaction = NULL;
	tdb->tdb2.access = NULL;
}

struct new_database {
	struct tdb_header hdr;
	struct tdb_freetable ftable;
};

/* initialise a new database */
static enum TDB_ERROR tdb_new_database(struct tdb_context *tdb,
				       struct tdb_attribute_seed *seed,
				       struct tdb_header *hdr)
{
	/* We make it up in memory, then write it out if not internal */
	struct new_database newdb;
	unsigned int magic_len;
	ssize_t rlen;
	enum TDB_ERROR ecode;

	/* Fill in the header */
	newdb.hdr.version = TDB_VERSION;
	if (seed)
		newdb.hdr.hash_seed = seed->seed;
	else
		newdb.hdr.hash_seed = random_number(tdb);
	newdb.hdr.hash_test = TDB_HASH_MAGIC;
	newdb.hdr.hash_test = tdb->hash_fn(&newdb.hdr.hash_test,
					   sizeof(newdb.hdr.hash_test),
					   newdb.hdr.hash_seed,
					   tdb->hash_data);
	newdb.hdr.recovery = 0;
	newdb.hdr.features_used = newdb.hdr.features_offered = TDB_FEATURE_MASK;
	newdb.hdr.seqnum = 0;
	memset(newdb.hdr.reserved, 0, sizeof(newdb.hdr.reserved));
	/* Initial hashes are empty. */
	memset(newdb.hdr.hashtable, 0, sizeof(newdb.hdr.hashtable));

	/* Free is empty. */
	newdb.hdr.free_table = offsetof(struct new_database, ftable);
	memset(&newdb.ftable, 0, sizeof(newdb.ftable));
	ecode = set_header(NULL, &newdb.ftable.hdr, TDB_FTABLE_MAGIC, 0,
			   sizeof(newdb.ftable) - sizeof(newdb.ftable.hdr),
			   sizeof(newdb.ftable) - sizeof(newdb.ftable.hdr),
			   0);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* Magic food */
	memset(newdb.hdr.magic_food, 0, sizeof(newdb.hdr.magic_food));
	strcpy(newdb.hdr.magic_food, TDB_MAGIC_FOOD);

	/* This creates an endian-converted database, as if read from disk */
	magic_len = sizeof(newdb.hdr.magic_food);
	tdb_convert(tdb,
		    (char *)&newdb.hdr + magic_len, sizeof(newdb) - magic_len);

	*hdr = newdb.hdr;

	if (tdb->flags & TDB_INTERNAL) {
		tdb->file->map_size = sizeof(newdb);
		tdb->file->map_ptr = malloc(tdb->file->map_size);
		if (!tdb->file->map_ptr) {
			return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					  "tdb_new_database:"
					  " failed to allocate");
		}
		memcpy(tdb->file->map_ptr, &newdb, tdb->file->map_size);
		return TDB_SUCCESS;
	}
	if (lseek(tdb->file->fd, 0, SEEK_SET) == -1) {
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "tdb_new_database:"
				  " failed to seek: %s", strerror(errno));
	}

	if (ftruncate(tdb->file->fd, 0) == -1) {
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "tdb_new_database:"
				  " failed to truncate: %s", strerror(errno));
	}

	rlen = write(tdb->file->fd, &newdb, sizeof(newdb));
	if (rlen != sizeof(newdb)) {
		if (rlen >= 0)
			errno = ENOSPC;
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "tdb_new_database: %zi writing header: %s",
				  rlen, strerror(errno));
	}
	return TDB_SUCCESS;
}

static enum TDB_ERROR tdb_new_file(struct tdb_context *tdb)
{
	tdb->file = malloc(sizeof(*tdb->file));
	if (!tdb->file)
		return tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
				  "tdb_open: cannot alloc tdb_file structure");
	tdb->file->num_lockrecs = 0;
	tdb->file->lockrecs = NULL;
	tdb->file->allrecord_lock.count = 0;
	tdb->file->refcnt = 1;
	tdb->file->map_ptr = NULL;
	return TDB_SUCCESS;
}

enum TDB_ERROR tdb_set_attribute(struct tdb_context *tdb,
				 const union tdb_attribute *attr)
{
	switch (attr->base.attr) {
	case TDB_ATTRIBUTE_LOG:
		tdb->log_fn = attr->log.fn;
		tdb->log_data = attr->log.data;
		break;
	case TDB_ATTRIBUTE_HASH:
	case TDB_ATTRIBUTE_SEED:
	case TDB_ATTRIBUTE_OPENHOOK:
	case TDB_ATTRIBUTE_TDB1_HASHSIZE:
		return tdb->last_error
			= tdb_logerr(tdb, TDB_ERR_EINVAL,
				     TDB_LOG_USE_ERROR,
				     "tdb_set_attribute:"
				     " cannot set %s after opening",
				     attr->base.attr == TDB_ATTRIBUTE_HASH
				     ? "TDB_ATTRIBUTE_HASH"
				     : attr->base.attr == TDB_ATTRIBUTE_SEED
				     ? "TDB_ATTRIBUTE_SEED"
				     : attr->base.attr == TDB_ATTRIBUTE_OPENHOOK
				     ? "TDB_ATTRIBUTE_OPENHOOK"
				     : "TDB_ATTRIBUTE_TDB1_HASHSIZE");
	case TDB_ATTRIBUTE_STATS:
		return tdb->last_error
			= tdb_logerr(tdb, TDB_ERR_EINVAL,
				     TDB_LOG_USE_ERROR,
				     "tdb_set_attribute:"
				     " cannot set TDB_ATTRIBUTE_STATS");
	case TDB_ATTRIBUTE_FLOCK:
		tdb->lock_fn = attr->flock.lock;
		tdb->unlock_fn = attr->flock.unlock;
		tdb->lock_data = attr->flock.data;
		break;
	default:
		return tdb->last_error
			= tdb_logerr(tdb, TDB_ERR_EINVAL,
				     TDB_LOG_USE_ERROR,
				     "tdb_set_attribute:"
				     " unknown attribute type %u",
				     attr->base.attr);
	}
	return TDB_SUCCESS;
}

enum TDB_ERROR tdb_get_attribute(struct tdb_context *tdb,
				 union tdb_attribute *attr)
{
	switch (attr->base.attr) {
	case TDB_ATTRIBUTE_LOG:
		if (!tdb->log_fn)
			return tdb->last_error = TDB_ERR_NOEXIST;
		attr->log.fn = tdb->log_fn;
		attr->log.data = tdb->log_data;
		break;
	case TDB_ATTRIBUTE_HASH:
		attr->hash.fn = tdb->hash_fn;
		attr->hash.data = tdb->hash_data;
		break;
	case TDB_ATTRIBUTE_SEED:
		if (tdb->flags & TDB_VERSION1)
			return tdb->last_error
				= tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
				     "tdb_get_attribute:"
				     " cannot get TDB_ATTRIBUTE_SEED"
				     " on TDB1 tdb.");
		attr->seed.seed = tdb->hash_seed;
		break;
	case TDB_ATTRIBUTE_OPENHOOK:
		if (!tdb->openhook)
			return tdb->last_error = TDB_ERR_NOEXIST;
		attr->openhook.fn = tdb->openhook;
		attr->openhook.data = tdb->openhook_data;
		break;
	case TDB_ATTRIBUTE_STATS: {
		size_t size = attr->stats.size;
		if (size > tdb->stats.size)
			size = tdb->stats.size;
		memcpy(&attr->stats, &tdb->stats, size);
		break;
	}
	case TDB_ATTRIBUTE_FLOCK:
		attr->flock.lock = tdb->lock_fn;
		attr->flock.unlock = tdb->unlock_fn;
		attr->flock.data = tdb->lock_data;
		break;
	case TDB_ATTRIBUTE_TDB1_HASHSIZE:
		if (!(tdb->flags & TDB_VERSION1))
			return tdb->last_error
				= tdb_logerr(tdb, TDB_ERR_EINVAL,
					     TDB_LOG_USE_ERROR,
				     "tdb_get_attribute:"
				     " cannot get TDB_ATTRIBUTE_TDB1_HASHSIZE"
				     " on TDB2 tdb.");
		attr->tdb1_hashsize.hsize = tdb->tdb1.header.hash_size;
		break;
	default:
		return tdb->last_error
			= tdb_logerr(tdb, TDB_ERR_EINVAL,
				     TDB_LOG_USE_ERROR,
				     "tdb_get_attribute:"
				     " unknown attribute type %u",
				     attr->base.attr);
	}
	attr->base.next = NULL;
	return TDB_SUCCESS;
}

void tdb_unset_attribute(struct tdb_context *tdb,
			 enum tdb_attribute_type type)
{
	switch (type) {
	case TDB_ATTRIBUTE_LOG:
		tdb->log_fn = NULL;
		break;
	case TDB_ATTRIBUTE_OPENHOOK:
		tdb->openhook = NULL;
		break;
	case TDB_ATTRIBUTE_HASH:
	case TDB_ATTRIBUTE_SEED:
	case TDB_ATTRIBUTE_TDB1_HASHSIZE:
		tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
			   "tdb_unset_attribute: cannot unset %s after opening",
			   type == TDB_ATTRIBUTE_HASH
			   ? "TDB_ATTRIBUTE_HASH"
			   : type == TDB_ATTRIBUTE_SEED
			   ? "TDB_ATTRIBUTE_SEED"
			   : "TDB_ATTRIBUTE_TDB1_HASHSIZE");
		break;
	case TDB_ATTRIBUTE_STATS:
		tdb_logerr(tdb, TDB_ERR_EINVAL,
			   TDB_LOG_USE_ERROR,
			   "tdb_unset_attribute:"
			   "cannot unset TDB_ATTRIBUTE_STATS");
		break;
	case TDB_ATTRIBUTE_FLOCK:
		tdb->lock_fn = tdb_fcntl_lock;
		tdb->unlock_fn = tdb_fcntl_unlock;
		break;
	default:
		tdb_logerr(tdb, TDB_ERR_EINVAL,
			   TDB_LOG_USE_ERROR,
			   "tdb_unset_attribute: unknown attribute type %u",
			   type);
	}
}

static bool is_tdb1(struct tdb1_header *hdr, const void *buf, ssize_t rlen)
{
	/* This code assumes we've tried to read entire tdb1 header. */
	BUILD_ASSERT(sizeof(*hdr) <= sizeof(struct tdb_header));

	if (rlen < (ssize_t)sizeof(*hdr)) {
		return false;
	}

	memcpy(hdr, buf, sizeof(*hdr));
	if (strcmp(hdr->magic_food, TDB_MAGIC_FOOD) != 0)
		return false;

	return hdr->version == TDB1_VERSION
		|| hdr->version == TDB1_BYTEREV(TDB1_VERSION);
}

struct tdb_context *tdb_open(const char *name, int tdb_flags,
			     int open_flags, mode_t mode,
			     union tdb_attribute *attr)
{
	struct tdb_context *tdb;
	struct stat st;
	int saved_errno = 0;
	uint64_t hash_test;
	unsigned v;
	ssize_t rlen;
	struct tdb_header hdr;
	struct tdb_attribute_seed *seed = NULL;
	struct tdb_attribute_tdb1_hashsize *hsize_attr = NULL;
	tdb_bool_err berr;
	enum TDB_ERROR ecode;
	int openlock;

	tdb = malloc(sizeof(*tdb) + (name ? strlen(name) + 1 : 0));
	if (!tdb) {
		/* Can't log this */
		errno = ENOMEM;
		return NULL;
	}
	/* Set name immediately for logging functions. */
	if (name) {
		tdb->name = strcpy((char *)(tdb + 1), name);
	} else {
		tdb->name = NULL;
	}
	tdb->flags = tdb_flags;
	tdb->log_fn = NULL;
	tdb->open_flags = open_flags;
	tdb->last_error = TDB_SUCCESS;
	tdb->file = NULL;
	tdb->openhook = NULL;
	tdb->lock_fn = tdb_fcntl_lock;
	tdb->unlock_fn = tdb_fcntl_unlock;
	tdb->hash_fn = tdb_jenkins_hash;
	memset(&tdb->stats, 0, sizeof(tdb->stats));
	tdb->stats.base.attr = TDB_ATTRIBUTE_STATS;
	tdb->stats.size = sizeof(tdb->stats);

	while (attr) {
		switch (attr->base.attr) {
		case TDB_ATTRIBUTE_HASH:
			tdb->hash_fn = attr->hash.fn;
			tdb->hash_data = attr->hash.data;
			break;
		case TDB_ATTRIBUTE_SEED:
			seed = &attr->seed;
			break;
		case TDB_ATTRIBUTE_OPENHOOK:
			tdb->openhook = attr->openhook.fn;
			tdb->openhook_data = attr->openhook.data;
			break;
		case TDB_ATTRIBUTE_TDB1_HASHSIZE:
			hsize_attr = &attr->tdb1_hashsize;
			break;
		default:
			/* These are set as normal. */
			ecode = tdb_set_attribute(tdb, attr);
			if (ecode != TDB_SUCCESS)
				goto fail;
		}
		attr = attr->base.next;
	}

	if (tdb_flags & ~(TDB_INTERNAL | TDB_NOLOCK | TDB_NOMMAP | TDB_CONVERT
			  | TDB_NOSYNC | TDB_SEQNUM | TDB_ALLOW_NESTING
			  | TDB_RDONLY | TDB_VERSION1)) {
		ecode = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
				   "tdb_open: unknown flags %u", tdb_flags);
		goto fail;
	}

	if (hsize_attr) {
		if (!(tdb_flags & TDB_VERSION1) ||
		    (!(tdb_flags & TDB_INTERNAL) && !(open_flags & O_CREAT))) {
			ecode = tdb_logerr(tdb, TDB_ERR_EINVAL,
					   TDB_LOG_USE_ERROR,
					   "tdb_open: can only use"
					   " TDB_ATTRIBUTE_TDB1_HASHSIZE when"
					   " creating a TDB_VERSION1 tdb");
			goto fail;
		}
	}

	if (seed) {
		if (tdb_flags & TDB_VERSION1) {
			ecode = tdb_logerr(tdb, TDB_ERR_EINVAL,
					   TDB_LOG_USE_ERROR,
					   "tdb_open:"
					   " cannot set TDB_ATTRIBUTE_SEED"
					   " on TDB1 tdb.");
			goto fail;
		} else if (!(tdb_flags & TDB_INTERNAL)
			   && !(open_flags & O_CREAT)) {
			ecode = tdb_logerr(tdb, TDB_ERR_EINVAL,
					   TDB_LOG_USE_ERROR,
					   "tdb_open:"
					   " cannot set TDB_ATTRIBUTE_SEED"
					   " without O_CREAT.");
			goto fail;
		}
	}

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		ecode = tdb_logerr(tdb, TDB_ERR_EINVAL, TDB_LOG_USE_ERROR,
				   "tdb_open: can't open tdb %s write-only",
				   name);
		goto fail;
	}

	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		openlock = F_RDLCK;
		tdb->flags |= TDB_RDONLY;
	} else {
		if (tdb_flags & TDB_RDONLY) {
			ecode = tdb_logerr(tdb, TDB_ERR_EINVAL,
					   TDB_LOG_USE_ERROR,
					   "tdb_open: can't use TDB_RDONLY"
					   " without O_RDONLY");
			goto fail;
		}
		openlock = F_WRLCK;
	}

	/* internal databases don't need any of the rest. */
	if (tdb->flags & TDB_INTERNAL) {
		tdb->flags |= (TDB_NOLOCK | TDB_NOMMAP);
		ecode = tdb_new_file(tdb);
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}
		tdb->file->fd = -1;
		if (tdb->flags & TDB_VERSION1)
			ecode = tdb1_new_database(tdb, hsize_attr);
		else {
			ecode = tdb_new_database(tdb, seed, &hdr);
			if (ecode == TDB_SUCCESS) {
				tdb_convert(tdb, &hdr.hash_seed,
					    sizeof(hdr.hash_seed));
				tdb->hash_seed = hdr.hash_seed;
				tdb2_context_init(tdb);
				tdb_ftable_init(tdb);
			}
		}
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}
		return tdb;
	}

	if (stat(name, &st) != -1)
		tdb->file = find_file(st.st_dev, st.st_ino);

	if (!tdb->file) {
		int fd;

		if ((fd = open(name, open_flags, mode)) == -1) {
			/* errno set by open(2) */
			saved_errno = errno;
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open: could not open file %s: %s",
				   name, strerror(errno));
			goto fail_errno;
		}

		/* on exec, don't inherit the fd */
		v = fcntl(fd, F_GETFD, 0);
		fcntl(fd, F_SETFD, v | FD_CLOEXEC);

		if (fstat(fd, &st) == -1) {
			saved_errno = errno;
			tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open: could not stat open %s: %s",
				   name, strerror(errno));
			close(fd);
			goto fail_errno;
		}

		ecode = tdb_new_file(tdb);
		if (ecode != TDB_SUCCESS) {
			close(fd);
			goto fail;
		}

		tdb->file->fd = fd;
		tdb->file->device = st.st_dev;
		tdb->file->inode = st.st_ino;
		tdb->file->map_ptr = NULL;
		tdb->file->map_size = 0;
 	}

	/* ensure there is only one process initialising at once */
	ecode = tdb_lock_open(tdb, openlock, TDB_LOCK_WAIT|TDB_LOCK_NOCHECK);
	if (ecode != TDB_SUCCESS) {
		saved_errno = errno;
		goto fail_errno;
	}

	/* call their open hook if they gave us one. */
	if (tdb->openhook) {
		ecode = tdb->openhook(tdb->file->fd, tdb->openhook_data);
		if (ecode != TDB_SUCCESS) {
			tdb_logerr(tdb, ecode, TDB_LOG_ERROR,
				   "tdb_open: open hook failed");
			goto fail;
		}
		open_flags |= O_CREAT;
	}

	/* If they used O_TRUNC, read will return 0. */
	rlen = pread(tdb->file->fd, &hdr, sizeof(hdr), 0);
	if (rlen == 0 && (open_flags & O_CREAT)) {
		if (tdb->flags & TDB_VERSION1) {
			ecode = tdb1_new_database(tdb, hsize_attr);
			if (ecode != TDB_SUCCESS)
				goto fail;
			goto finished;
		}
		ecode = tdb_new_database(tdb, seed, &hdr);
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}
	} else if (rlen < 0) {
		ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open: error %s reading %s",
				   strerror(errno), name);
		goto fail;
	} else if (rlen < sizeof(hdr)
		   || strcmp(hdr.magic_food, TDB_MAGIC_FOOD) != 0) {
		if (is_tdb1(&tdb->tdb1.header, &hdr, rlen)) {
			ecode = tdb1_open(tdb);
			if (!ecode)
				goto finished;
			goto fail;
		}
		ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open: %s is not a tdb file", name);
		goto fail;
	}

	if (hdr.version != TDB_VERSION) {
		if (hdr.version == bswap_64(TDB_VERSION))
			tdb->flags |= TDB_CONVERT;
		else {
			if (is_tdb1(&tdb->tdb1.header, &hdr, rlen)) {
				ecode = tdb1_open(tdb);
				if (!ecode)
					goto finished;
				goto fail;
			}
			/* wrong version */
			ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					   "tdb_open:"
					   " %s is unknown version 0x%llx",
					   name, (long long)hdr.version);
			goto fail;
		}
	} else if (tdb->flags & TDB_CONVERT) {
		ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open:"
				   " %s does not need TDB_CONVERT",
				   name);
		goto fail;
	}

	if (tdb->flags & TDB_VERSION1) {
		ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open:"
				   " %s does not need TDB_VERSION1",
				   name);
		goto fail;
	}

	tdb2_context_init(tdb);

	tdb_convert(tdb, &hdr, sizeof(hdr));
	tdb->hash_seed = hdr.hash_seed;
	hash_test = TDB_HASH_MAGIC;
	hash_test = tdb_hash(tdb, &hash_test, sizeof(hash_test));
	if (hdr.hash_test != hash_test) {
		/* wrong hash variant */
		ecode = tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				   "tdb_open:"
				   " %s uses a different hash function",
				   name);
		goto fail;
	}

	/* Clear any features we don't understand. */
 	if ((open_flags & O_ACCMODE) != O_RDONLY) {
		hdr.features_used &= TDB_FEATURE_MASK;
		ecode = tdb_write_convert(tdb, offsetof(struct tdb_header,
							features_used),
					  &hdr.features_used,
					  sizeof(hdr.features_used));
		if (ecode != TDB_SUCCESS)
			goto fail;
	}

finished:
	if (tdb->flags & TDB_VERSION1) {
		/* if needed, run recovery */
		if (tdb1_transaction_recover(tdb) == -1) {
			ecode = tdb->last_error;
			goto fail;
		}
	}

	tdb_unlock_open(tdb, openlock);

	/* This makes sure we have current map_size and mmap. */
	if (tdb->flags & TDB_VERSION1) {
		ecode = tdb1_probe_length(tdb);
	} else {
		ecode = tdb->tdb2.io->oob(tdb, tdb->file->map_size + 1, true);
	}
	if (unlikely(ecode != TDB_SUCCESS))
		goto fail;

	if (!(tdb->flags & TDB_VERSION1)) {
		/* Now it's fully formed, recover if necessary. */
		berr = tdb_needs_recovery(tdb);
		if (unlikely(berr != false)) {
			if (berr < 0) {
				ecode = berr;
				goto fail;
			}
			ecode = tdb_lock_and_recover(tdb);
			if (ecode != TDB_SUCCESS) {
				goto fail;
			}
		}

		ecode = tdb_ftable_init(tdb);
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}
	}

	tdb->next = tdbs;
	tdbs = tdb;
	return tdb;

 fail:
	/* Map ecode to some logical errno. */
	switch (ecode) {
	case TDB_ERR_CORRUPT:
	case TDB_ERR_IO:
		saved_errno = EIO;
		break;
	case TDB_ERR_LOCK:
		saved_errno = EWOULDBLOCK;
		break;
	case TDB_ERR_OOM:
		saved_errno = ENOMEM;
		break;
	case TDB_ERR_EINVAL:
		saved_errno = EINVAL;
		break;
	default:
		saved_errno = EINVAL;
		break;
	}

fail_errno:
#ifdef TDB_TRACE
	close(tdb->tracefd);
#endif
	if (tdb->file) {
		tdb_lock_cleanup(tdb);
		if (--tdb->file->refcnt == 0) {
			assert(tdb->file->num_lockrecs == 0);
			if (tdb->file->map_ptr) {
				if (tdb->flags & TDB_INTERNAL) {
					free(tdb->file->map_ptr);
				} else
					tdb_munmap(tdb->file);
			}
			if (close(tdb->file->fd) != 0)
				tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					   "tdb_open: failed to close tdb fd"
					   " on error: %s", strerror(errno));
			free(tdb->file->lockrecs);
			free(tdb->file);
		}
	}

	free(tdb);
	errno = saved_errno;
	return NULL;
}

int tdb_close(struct tdb_context *tdb)
{
	int ret = 0;
	struct tdb_context **i;

	tdb_trace(tdb, "tdb_close");

	if (tdb->flags & TDB_VERSION1) {
		if (tdb->tdb1.transaction) {
			tdb1_transaction_cancel(tdb);
		}
	} else {
		if (tdb->tdb2.transaction) {
			tdb_transaction_cancel(tdb);
		}
	}

	if (tdb->file->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			free(tdb->file->map_ptr);
		else
			tdb_munmap(tdb->file);
	}
	if (tdb->file) {
		tdb_lock_cleanup(tdb);
		if (--tdb->file->refcnt == 0) {
			ret = close(tdb->file->fd);
			free(tdb->file->lockrecs);
			free(tdb->file);
		}
	}

	/* Remove from tdbs list */
	for (i = &tdbs; *i; i = &(*i)->next) {
		if (*i == tdb) {
			*i = tdb->next;
			break;
		}
	}

#ifdef TDB_TRACE
	close(tdb->tracefd);
#endif
	free(tdb);

	return ret;
}

void tdb_foreach_(int (*fn)(struct tdb_context *, void *), void *p)
{
	struct tdb_context *i;

	for (i = tdbs; i; i = i->next) {
		if (fn(i, p) != 0)
			break;
	}
}
