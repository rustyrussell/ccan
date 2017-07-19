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

/* all tdbs, to detect double-opens (fcntl file don't nest!) */
static struct ntdb_context *tdbs = NULL;

static struct ntdb_file *find_file(dev_t device, ino_t ino)
{
	struct ntdb_context *i;

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

static uint32_t random_number(struct ntdb_context *ntdb)
{
	int fd;
	uint32_t ret = 0;
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
		char cmd[2] = { 1, sizeof(uint32_t) };
		if (write(fd, cmd, sizeof(cmd)) == sizeof(cmd)) {
			char reply[1 + sizeof(uint32_t)];
			int r = read(fd, reply, sizeof(reply));
			if (r > 1) {
				/* Copy at least some bytes. */
				memcpy(&ret, reply+1, r - 1);
				if (reply[0] == sizeof(uint32_t)
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
	ntdb_logerr(ntdb, NTDB_SUCCESS, NTDB_LOG_WARNING,
		   "ntdb_open: random from getpid and time");
	return ret;
}

static void ntdb_context_init(struct ntdb_context *ntdb)
{
	/* Initialize the NTDB fields here */
	ntdb_io_init(ntdb);
	ntdb->transaction = NULL;
	ntdb->access = NULL;
}

/* initialise a new database:
 *
 *	struct ntdb_header;
 *	struct {
 *		struct ntdb_used_record hash_header;
 *		ntdb_off_t hash_buckets[1 << ntdb->hash_bits];
 *	} hash;
 *	struct ntdb_freetable ftable;
 *	struct {
 *		struct ntdb_free_record free_header;
 *		char forty_three[...];
 *	} remainder;
 */
#define NEW_DATABASE_HDR_SIZE(hbits)					\
	(sizeof(struct ntdb_header)					\
	 + sizeof(struct ntdb_used_record) + (sizeof(ntdb_off_t) << hbits) \
	 + sizeof(struct ntdb_freetable)				\
	 + sizeof(struct ntdb_free_record))

static enum NTDB_ERROR ntdb_new_database(struct ntdb_context *ntdb,
					 struct ntdb_attribute_seed *seed,
					 struct ntdb_header *rhdr)
{
	/* We make it up in memory, then write it out if not internal */
	struct ntdb_freetable *ftable;
	struct ntdb_used_record *htable;
	struct ntdb_header *hdr;
	struct ntdb_free_record *remainder;
	char *mem;
	unsigned int magic_len;
	ssize_t rlen;
	size_t dbsize, hashsize, hdrsize, remaindersize;
	enum NTDB_ERROR ecode;

	hashsize = sizeof(ntdb_off_t) << ntdb->hash_bits;

	/* Always make db a multiple of NTDB_PGSIZE */
	hdrsize = NEW_DATABASE_HDR_SIZE(ntdb->hash_bits);
	dbsize = (hdrsize + NTDB_PGSIZE-1) & ~(NTDB_PGSIZE-1);

	mem = ntdb->alloc_fn(ntdb, dbsize, ntdb->alloc_data);
	if (!mem) {
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				   "ntdb_new_database: failed to allocate");
	}

	hdr = (void *)mem;
	htable = (void *)(mem + sizeof(*hdr));
	ftable = (void *)(mem + sizeof(*hdr) + sizeof(*htable) + hashsize);
	remainder = (void *)(mem + sizeof(*hdr) + sizeof(*htable) + hashsize
			     + sizeof(*ftable));

	/* Fill in the header */
	hdr->version = NTDB_VERSION;
	if (seed)
		hdr->hash_seed = seed->seed;
	else
		hdr->hash_seed = random_number(ntdb);
	hdr->hash_test = NTDB_HASH_MAGIC;
	hdr->hash_test = ntdb->hash_fn(&hdr->hash_test,
				       sizeof(hdr->hash_test),
				       hdr->hash_seed,
				       ntdb->hash_data);
	hdr->hash_bits = ntdb->hash_bits;
	hdr->recovery = 0;
	hdr->features_used = hdr->features_offered = NTDB_FEATURE_MASK;
	hdr->seqnum = 0;
	hdr->capabilities = 0;
	memset(hdr->reserved, 0, sizeof(hdr->reserved));

	/* Hash is all zero after header. */
	set_header(NULL, htable, NTDB_HTABLE_MAGIC, 0, hashsize, hashsize);
	memset(htable + 1, 0, hashsize);

	/* Free is empty. */
	hdr->free_table = (char *)ftable - (char *)hdr;
	memset(ftable, 0, sizeof(*ftable));
	ecode = set_header(NULL, &ftable->hdr, NTDB_FTABLE_MAGIC, 0,
			   sizeof(*ftable) - sizeof(ftable->hdr),
			   sizeof(*ftable) - sizeof(ftable->hdr));
	if (ecode != NTDB_SUCCESS) {
		goto out;
	}

	/* Rest of database is a free record, containing junk. */
	remaindersize = dbsize - hdrsize;
	remainder->ftable_and_len
		= (remaindersize + sizeof(*remainder)
		   - sizeof(struct ntdb_used_record));
	remainder->next = 0;
	remainder->magic_and_prev
		= (NTDB_FREE_MAGIC << (64-NTDB_OFF_UPPER_STEAL))
		| ((char *)remainder - (char *)hdr);
	memset(remainder + 1, 0x43, remaindersize);

	/* Put in our single free entry. */
	ftable->buckets[size_to_bucket(remaindersize)] =
		(char *)remainder - (char *)hdr;

	/* Magic food */
	memset(hdr->magic_food, 0, sizeof(hdr->magic_food));
	strcpy(hdr->magic_food, NTDB_MAGIC_FOOD);

	/* This creates an endian-converted database, as if read from disk */
	magic_len = sizeof(hdr->magic_food);
	ntdb_convert(ntdb, (char *)hdr + magic_len, hdrsize - magic_len);

	/* Return copy of header. */
	*rhdr = *hdr;

	if (ntdb->flags & NTDB_INTERNAL) {
		ntdb->file->map_size = dbsize;
		ntdb->file->map_ptr = hdr;
		return NTDB_SUCCESS;
	}
	if (lseek(ntdb->file->fd, 0, SEEK_SET) == -1) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				    "ntdb_new_database:"
				    " failed to seek: %s", strerror(errno));
		goto out;
	}

	if (ftruncate(ntdb->file->fd, 0) == -1) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				    "ntdb_new_database:"
				    " failed to truncate: %s", strerror(errno));
		goto out;
	}

	rlen = write(ntdb->file->fd, hdr, dbsize);
	if (rlen != dbsize) {
		if (rlen >= 0)
			errno = ENOSPC;
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				    "ntdb_new_database: %zi writing header: %s",
				    rlen, strerror(errno));
		goto out;
	}

out:
	ntdb->free_fn(hdr, ntdb->alloc_data);
	return ecode;
}

static enum NTDB_ERROR ntdb_new_file(struct ntdb_context *ntdb)
{
	ntdb->file = ntdb->alloc_fn(NULL, sizeof(*ntdb->file), ntdb->alloc_data);
	if (!ntdb->file)
		return ntdb_logerr(ntdb, NTDB_ERR_OOM, NTDB_LOG_ERROR,
				  "ntdb_open: cannot alloc ntdb_file structure");
	ntdb->file->num_lockrecs = 0;
	ntdb->file->lockrecs = NULL;
	ntdb->file->allrecord_lock.count = 0;
	ntdb->file->refcnt = 1;
	ntdb->file->map_ptr = NULL;
	ntdb->file->direct_count = 0;
	ntdb->file->old_mmaps = NULL;
	return NTDB_SUCCESS;
}

_PUBLIC_ enum NTDB_ERROR ntdb_set_attribute(struct ntdb_context *ntdb,
				 const union ntdb_attribute *attr)
{
	switch (attr->base.attr) {
	case NTDB_ATTRIBUTE_LOG:
		ntdb->log_fn = attr->log.fn;
		ntdb->log_data = attr->log.data;
		break;
	case NTDB_ATTRIBUTE_HASH:
	case NTDB_ATTRIBUTE_SEED:
	case NTDB_ATTRIBUTE_OPENHOOK:
	case NTDB_ATTRIBUTE_HASHSIZE:
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
				   NTDB_LOG_USE_ERROR,
				   "ntdb_set_attribute:"
				   " cannot set %s after opening",
				   attr->base.attr == NTDB_ATTRIBUTE_HASH
				   ? "NTDB_ATTRIBUTE_HASH"
				   : attr->base.attr == NTDB_ATTRIBUTE_SEED
				   ? "NTDB_ATTRIBUTE_SEED"
				   : attr->base.attr == NTDB_ATTRIBUTE_OPENHOOK
				   ? "NTDB_ATTRIBUTE_OPENHOOK"
				   : "NTDB_ATTRIBUTE_HASHSIZE");
	case NTDB_ATTRIBUTE_STATS:
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
				   NTDB_LOG_USE_ERROR,
				   "ntdb_set_attribute:"
				   " cannot set NTDB_ATTRIBUTE_STATS");
	case NTDB_ATTRIBUTE_FLOCK:
		ntdb->lock_fn = attr->flock.lock;
		ntdb->unlock_fn = attr->flock.unlock;
		ntdb->lock_data = attr->flock.data;
		break;
	case NTDB_ATTRIBUTE_ALLOCATOR:
		ntdb->alloc_fn = attr->alloc.alloc;
		ntdb->expand_fn = attr->alloc.expand;
		ntdb->free_fn = attr->alloc.free;
		ntdb->alloc_data = attr->alloc.priv_data;
		break;
	default:
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
				   NTDB_LOG_USE_ERROR,
				   "ntdb_set_attribute:"
				   " unknown attribute type %u",
				   attr->base.attr);
	}
	return NTDB_SUCCESS;
}

_PUBLIC_ enum NTDB_ERROR ntdb_get_attribute(struct ntdb_context *ntdb,
				 union ntdb_attribute *attr)
{
	switch (attr->base.attr) {
	case NTDB_ATTRIBUTE_LOG:
		if (!ntdb->log_fn)
			return NTDB_ERR_NOEXIST;
		attr->log.fn = ntdb->log_fn;
		attr->log.data = ntdb->log_data;
		break;
	case NTDB_ATTRIBUTE_HASH:
		attr->hash.fn = ntdb->hash_fn;
		attr->hash.data = ntdb->hash_data;
		break;
	case NTDB_ATTRIBUTE_SEED:
		attr->seed.seed = ntdb->hash_seed;
		break;
	case NTDB_ATTRIBUTE_OPENHOOK:
		if (!ntdb->openhook)
			return NTDB_ERR_NOEXIST;
		attr->openhook.fn = ntdb->openhook;
		attr->openhook.data = ntdb->openhook_data;
		break;
	case NTDB_ATTRIBUTE_STATS: {
		size_t size = attr->stats.size;
		if (size > ntdb->stats.size)
			size = ntdb->stats.size;
		memcpy(&attr->stats, &ntdb->stats, size);
		break;
	}
	case NTDB_ATTRIBUTE_FLOCK:
		attr->flock.lock = ntdb->lock_fn;
		attr->flock.unlock = ntdb->unlock_fn;
		attr->flock.data = ntdb->lock_data;
		break;
	case NTDB_ATTRIBUTE_ALLOCATOR:
		attr->alloc.alloc = ntdb->alloc_fn;
		attr->alloc.expand = ntdb->expand_fn;
		attr->alloc.free = ntdb->free_fn;
		attr->alloc.priv_data = ntdb->alloc_data;
		break;
	case NTDB_ATTRIBUTE_HASHSIZE:
		attr->hashsize.size = 1 << ntdb->hash_bits;
		break;
	default:
		return ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
				   NTDB_LOG_USE_ERROR,
				   "ntdb_get_attribute:"
				   " unknown attribute type %u",
				   attr->base.attr);
	}
	attr->base.next = NULL;
	return NTDB_SUCCESS;
}

_PUBLIC_ void ntdb_unset_attribute(struct ntdb_context *ntdb,
			 enum ntdb_attribute_type type)
{
	switch (type) {
	case NTDB_ATTRIBUTE_LOG:
		ntdb->log_fn = NULL;
		break;
	case NTDB_ATTRIBUTE_OPENHOOK:
		ntdb->openhook = NULL;
		break;
	case NTDB_ATTRIBUTE_HASH:
	case NTDB_ATTRIBUTE_SEED:
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
			   "ntdb_unset_attribute: cannot unset %s after opening",
			   type == NTDB_ATTRIBUTE_HASH
			   ? "NTDB_ATTRIBUTE_HASH"
			   : "NTDB_ATTRIBUTE_SEED");
		break;
	case NTDB_ATTRIBUTE_STATS:
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
			   NTDB_LOG_USE_ERROR,
			   "ntdb_unset_attribute:"
			   "cannot unset NTDB_ATTRIBUTE_STATS");
		break;
	case NTDB_ATTRIBUTE_FLOCK:
		ntdb->lock_fn = ntdb_fcntl_lock;
		ntdb->unlock_fn = ntdb_fcntl_unlock;
		break;
	default:
		ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
			   NTDB_LOG_USE_ERROR,
			   "ntdb_unset_attribute: unknown attribute type %u",
			   type);
	}
}

/* The top three bits of the capability tell us whether it matters. */
enum NTDB_ERROR unknown_capability(struct ntdb_context *ntdb, const char *caller,
				  ntdb_off_t type)
{
	if (type & NTDB_CAP_NOOPEN) {
		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "%s: file has unknown capability %llu",
				  caller, type & NTDB_CAP_NOOPEN);
	}

	if ((type & NTDB_CAP_NOWRITE) && !(ntdb->flags & NTDB_RDONLY)) {
		return ntdb_logerr(ntdb, NTDB_ERR_RDONLY, NTDB_LOG_ERROR,
				  "%s: file has unknown capability %llu"
				  " (cannot write to it)",
				  caller, type & NTDB_CAP_NOOPEN);
	}

	if (type & NTDB_CAP_NOCHECK) {
		ntdb->flags |= NTDB_CANT_CHECK;
	}
	return NTDB_SUCCESS;
}

static enum NTDB_ERROR capabilities_ok(struct ntdb_context *ntdb,
				      ntdb_off_t capabilities)
{
	ntdb_off_t off, next;
	enum NTDB_ERROR ecode = NTDB_SUCCESS;
	const struct ntdb_capability *cap;

	/* Check capability list. */
	for (off = capabilities; off && ecode == NTDB_SUCCESS; off = next) {
		cap = ntdb_access_read(ntdb, off, sizeof(*cap), true);
		if (NTDB_PTR_IS_ERR(cap)) {
			return NTDB_PTR_ERR(cap);
		}

		switch (cap->type & NTDB_CAP_TYPE_MASK) {
		/* We don't understand any capabilities (yet). */
		default:
			ecode = unknown_capability(ntdb, "ntdb_open", cap->type);
		}
		next = cap->next;
		ntdb_access_release(ntdb, cap);
	}
	return ecode;
}

static void *default_alloc(const void *owner, size_t len, void *priv_data)
{
	return malloc(len);
}

static void *default_expand(void *ptr, size_t len, void *priv_data)
{
	return realloc(ptr, len);
}

static void default_free(void *ptr, void *priv_data)
{
	free(ptr);
}

/* First allocation needs manual search of attributes. */
static struct ntdb_context *alloc_ntdb(const union ntdb_attribute *attr,
				       const char *name)
{
	size_t len = sizeof(struct ntdb_context) + strlen(name) + 1;

	while (attr) {
		if  (attr->base.attr == NTDB_ATTRIBUTE_ALLOCATOR) {
			return attr->alloc.alloc(NULL, len,
						 attr->alloc.priv_data);
		}
		attr = attr->base.next;
	}
	return default_alloc(NULL, len, NULL);
}

static unsigned int next_pow2(uint64_t size)
{
	unsigned int bits = 1;

	while ((1ULL << bits) < size)
		bits++;
	return bits;
}

_PUBLIC_ struct ntdb_context *ntdb_open(const char *name, int ntdb_flags,
					int open_flags, mode_t mode,
					union ntdb_attribute *attr)
{
	struct ntdb_context *ntdb;
	struct stat st;
	int saved_errno = 0;
	uint64_t hash_test;
	unsigned v;
	ssize_t rlen;
	struct ntdb_header hdr;
	struct ntdb_attribute_seed *seed = NULL;
	ntdb_bool_err berr;
	enum NTDB_ERROR ecode;
	int openlock;

	ntdb = alloc_ntdb(attr, name);
	if (!ntdb) {
		/* Can't log this */
		errno = ENOMEM;
		return NULL;
	}
	/* Set name immediately for logging functions. */
	ntdb->name = strcpy((char *)(ntdb + 1), name);
	ntdb->flags = ntdb_flags;
	ntdb->log_fn = NULL;
	ntdb->open_flags = open_flags;
	ntdb->file = NULL;
	ntdb->openhook = NULL;
	ntdb->lock_fn = ntdb_fcntl_lock;
	ntdb->unlock_fn = ntdb_fcntl_unlock;
	ntdb->hash_fn = ntdb_jenkins_hash;
	memset(&ntdb->stats, 0, sizeof(ntdb->stats));
	ntdb->stats.base.attr = NTDB_ATTRIBUTE_STATS;
	ntdb->stats.size = sizeof(ntdb->stats);
	ntdb->alloc_fn = default_alloc;
	ntdb->expand_fn = default_expand;
	ntdb->free_fn = default_free;
	ntdb->hash_bits = NTDB_DEFAULT_HBITS; /* 64k of hash by default. */

	while (attr) {
		switch (attr->base.attr) {
		case NTDB_ATTRIBUTE_HASH:
			ntdb->hash_fn = attr->hash.fn;
			ntdb->hash_data = attr->hash.data;
			break;
		case NTDB_ATTRIBUTE_SEED:
			seed = &attr->seed;
			break;
		case NTDB_ATTRIBUTE_OPENHOOK:
			ntdb->openhook = attr->openhook.fn;
			ntdb->openhook_data = attr->openhook.data;
			break;
		case NTDB_ATTRIBUTE_HASHSIZE:
			ntdb->hash_bits = next_pow2(attr->hashsize.size);
			if (ntdb->hash_bits > 31) {
				ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
						    NTDB_LOG_USE_ERROR,
						    "ntdb_open: hash_size %u"
						    " too large",
						    attr->hashsize.size);
				goto fail;
			}
			break;
		default:
			/* These are set as normal. */
			ecode = ntdb_set_attribute(ntdb, attr);
			if (ecode != NTDB_SUCCESS)
				goto fail;
		}
		attr = attr->base.next;
	}

	if (ntdb_flags & ~(NTDB_INTERNAL | NTDB_NOLOCK | NTDB_NOMMAP | NTDB_CONVERT
			  | NTDB_NOSYNC | NTDB_SEQNUM | NTDB_ALLOW_NESTING
			  | NTDB_RDONLY)) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				   "ntdb_open: unknown flags %u", ntdb_flags);
		goto fail;
	}

	if (seed) {
		if (!(ntdb_flags & NTDB_INTERNAL) && !(open_flags & O_CREAT)) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
					   NTDB_LOG_USE_ERROR,
					   "ntdb_open:"
					   " cannot set NTDB_ATTRIBUTE_SEED"
					   " without O_CREAT.");
			goto fail;
		}
	}

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL, NTDB_LOG_USE_ERROR,
				   "ntdb_open: can't open ntdb %s write-only",
				   name);
		goto fail;
	}

	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		openlock = F_RDLCK;
		ntdb->flags |= NTDB_RDONLY;
	} else {
		if (ntdb_flags & NTDB_RDONLY) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_EINVAL,
					   NTDB_LOG_USE_ERROR,
					   "ntdb_open: can't use NTDB_RDONLY"
					   " without O_RDONLY");
			goto fail;
		}
		openlock = F_WRLCK;
	}

	/* internal databases don't need any of the rest. */
	if (ntdb->flags & NTDB_INTERNAL) {
		ntdb->flags |= (NTDB_NOLOCK | NTDB_NOMMAP);
		ecode = ntdb_new_file(ntdb);
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}
		ntdb->file->fd = -1;
		ecode = ntdb_new_database(ntdb, seed, &hdr);
		if (ecode == NTDB_SUCCESS) {
			ntdb_convert(ntdb, &hdr.hash_seed,
				    sizeof(hdr.hash_seed));
			ntdb->hash_seed = hdr.hash_seed;
			ntdb_context_init(ntdb);
			ntdb_ftable_init(ntdb);
		}
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}
		return ntdb;
	}

	if (stat(name, &st) != -1)
		ntdb->file = find_file(st.st_dev, st.st_ino);

	if (!ntdb->file) {
		ecode = ntdb_new_file(ntdb);
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}

		/* Set this now, as ntdb_nest_lock examines it. */
		ntdb->file->map_size = 0;

		if ((ntdb->file->fd = open(name, open_flags, mode)) == -1) {
			enum ntdb_log_level lvl;
			/* errno set by open(2) */
			saved_errno = errno;

			/* Probing for files like this is a common pattern. */
			if (!(open_flags & O_CREAT) && errno == ENOENT) {
				lvl = NTDB_LOG_WARNING;
			} else {
				lvl = NTDB_LOG_ERROR;
			}
			ntdb_logerr(ntdb, NTDB_ERR_IO, lvl,
				   "ntdb_open: could not open file %s: %s",
				   name, strerror(errno));

			goto fail_errno;
		}

		/* ensure there is only one process initialising at once:
		 * do it immediately to reduce the create/openlock race. */
		ecode = ntdb_lock_open(ntdb, openlock,
				       NTDB_LOCK_WAIT|NTDB_LOCK_NOCHECK);
		if (ecode != NTDB_SUCCESS) {
			saved_errno = errno;
			goto fail_errno;
		}

		/* on exec, don't inherit the fd */
		v = fcntl(ntdb->file->fd, F_GETFD, 0);
		fcntl(ntdb->file->fd, F_SETFD, v | FD_CLOEXEC);

		if (fstat(ntdb->file->fd, &st) == -1) {
			saved_errno = errno;
			ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				   "ntdb_open: could not stat open %s: %s",
				   name, strerror(errno));
			goto fail_errno;
		}

		ntdb->file->device = st.st_dev;
		ntdb->file->inode = st.st_ino;

		/* call their open hook if they gave us one. */
		if (ntdb->openhook) {
			ecode = ntdb->openhook(ntdb->file->fd, ntdb->openhook_data);
			if (ecode != NTDB_SUCCESS) {
				ntdb_logerr(ntdb, ecode, NTDB_LOG_ERROR,
					    "ntdb_open: open hook failed");
				goto fail;
			}
			open_flags |= O_CREAT;
		}
	} else {
		/* ensure there is only one process initialising at once */
		ecode = ntdb_lock_open(ntdb, openlock,
				       NTDB_LOCK_WAIT|NTDB_LOCK_NOCHECK);
		if (ecode != NTDB_SUCCESS) {
			saved_errno = errno;
			goto fail_errno;
		}
	}

	/* If they used O_TRUNC, read will return 0. */
	rlen = pread(ntdb->file->fd, &hdr, sizeof(hdr), 0);
	if (rlen == 0 && (open_flags & O_CREAT)) {
		ecode = ntdb_new_database(ntdb, seed, &hdr);
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}
	} else if (rlen < 0) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				   "ntdb_open: error %s reading %s",
				   strerror(errno), name);
		goto fail;
	} else if (rlen < sizeof(hdr)
		   || strcmp(hdr.magic_food, NTDB_MAGIC_FOOD) != 0) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				   "ntdb_open: %s is not a ntdb file", name);
		goto fail;
	}

	if (hdr.version != NTDB_VERSION) {
		if (hdr.version == bswap_64(NTDB_VERSION))
			ntdb->flags |= NTDB_CONVERT;
		else {
			/* wrong version */
			ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					   "ntdb_open:"
					   " %s is unknown version 0x%llx",
					   name, (long long)hdr.version);
			goto fail;
		}
	} else if (ntdb->flags & NTDB_CONVERT) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				   "ntdb_open:"
				   " %s does not need NTDB_CONVERT",
				   name);
		goto fail;
	}

	ntdb_context_init(ntdb);

	ntdb_convert(ntdb, &hdr, sizeof(hdr));
	ntdb->hash_bits = hdr.hash_bits;
	ntdb->hash_seed = hdr.hash_seed;
	hash_test = NTDB_HASH_MAGIC;
	hash_test = ntdb_hash(ntdb, &hash_test, sizeof(hash_test));
	if (hdr.hash_test != hash_test) {
		/* wrong hash variant */
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				   "ntdb_open:"
				   " %s uses a different hash function",
				   name);
		goto fail;
	}

	ecode = capabilities_ok(ntdb, hdr.capabilities);
	if (ecode != NTDB_SUCCESS) {
		goto fail;
	}

	/* Clear any features we don't understand. */
	if ((open_flags & O_ACCMODE) != O_RDONLY) {
		hdr.features_used &= NTDB_FEATURE_MASK;
		ecode = ntdb_write_convert(ntdb, offsetof(struct ntdb_header,
							features_used),
					  &hdr.features_used,
					  sizeof(hdr.features_used));
		if (ecode != NTDB_SUCCESS)
			goto fail;
	}

	ntdb_unlock_open(ntdb, openlock);

	/* This makes sure we have current map_size and mmap. */
	ecode = ntdb_oob(ntdb, ntdb->file->map_size, 1, true);
	if (unlikely(ecode != NTDB_SUCCESS))
		goto fail;

	if (ntdb->file->map_size % NTDB_PGSIZE != 0) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				    "ntdb_open:"
				    " %s size %llu isn't a multiple of %u",
				    name, (long long)ntdb->file->map_size,
				    NTDB_PGSIZE);
		goto fail;
	}

	/* Now it's fully formed, recover if necessary. */
	berr = ntdb_needs_recovery(ntdb);
	if (unlikely(berr != false)) {
		if (berr < 0) {
			ecode = NTDB_OFF_TO_ERR(berr);
			goto fail;
		}
		ecode = ntdb_lock_and_recover(ntdb);
		if (ecode != NTDB_SUCCESS) {
			goto fail;
		}
	}

	ecode = ntdb_ftable_init(ntdb);
	if (ecode != NTDB_SUCCESS) {
		goto fail;
	}

	ntdb->next = tdbs;
	tdbs = ntdb;
	return ntdb;

 fail:
	/* Map ecode to some logical errno. */
	switch (NTDB_ERR_TO_OFF(ecode)) {
	case NTDB_ERR_TO_OFF(NTDB_ERR_CORRUPT):
	case NTDB_ERR_TO_OFF(NTDB_ERR_IO):
		saved_errno = EIO;
		break;
	case NTDB_ERR_TO_OFF(NTDB_ERR_LOCK):
		saved_errno = EWOULDBLOCK;
		break;
	case NTDB_ERR_TO_OFF(NTDB_ERR_OOM):
		saved_errno = ENOMEM;
		break;
	case NTDB_ERR_TO_OFF(NTDB_ERR_EINVAL):
		saved_errno = EINVAL;
		break;
	default:
		saved_errno = EINVAL;
		break;
	}

fail_errno:
#ifdef NTDB_TRACE
	close(ntdb->tracefd);
#endif
	if (ntdb->file) {
		ntdb_lock_cleanup(ntdb);
		if (--ntdb->file->refcnt == 0) {
			assert(ntdb->file->num_lockrecs == 0);
			if (ntdb->file->map_ptr) {
				if (ntdb->flags & NTDB_INTERNAL) {
					ntdb->free_fn(ntdb->file->map_ptr,
						      ntdb->alloc_data);
				} else
					ntdb_munmap(ntdb);
			}
			if (ntdb->file->fd != -1 && close(ntdb->file->fd) != 0)
				ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
					   "ntdb_open: failed to close ntdb fd"
					   " on error: %s", strerror(errno));
			ntdb->free_fn(ntdb->file->lockrecs, ntdb->alloc_data);
			ntdb->free_fn(ntdb->file, ntdb->alloc_data);
		}
	}

	ntdb->free_fn(ntdb, ntdb->alloc_data);
	errno = saved_errno;
	return NULL;
}

_PUBLIC_ int ntdb_close(struct ntdb_context *ntdb)
{
	int ret = 0;
	struct ntdb_context **i;

	ntdb_trace(ntdb, "ntdb_close");

	if (ntdb->transaction) {
		ntdb_transaction_cancel(ntdb);
	}

	ntdb_lock_cleanup(ntdb);
	if (--ntdb->file->refcnt == 0) {
		if (ntdb->file->map_ptr) {
			if (ntdb->flags & NTDB_INTERNAL) {
				ntdb->free_fn(ntdb->file->map_ptr,
					      ntdb->alloc_data);
			} else {
				ntdb_munmap(ntdb);
			}
		}
		ret = close(ntdb->file->fd);
		ntdb->free_fn(ntdb->file->lockrecs, ntdb->alloc_data);
		ntdb->free_fn(ntdb->file, ntdb->alloc_data);
	}

	/* Remove from tdbs list */
	for (i = &tdbs; *i; i = &(*i)->next) {
		if (*i == ntdb) {
			*i = ntdb->next;
			break;
		}
	}

#ifdef NTDB_TRACE
	close(ntdb->tracefd);
#endif
	ntdb->free_fn(ntdb, ntdb->alloc_data);

	return ret;
}

_PUBLIC_ void ntdb_foreach_(int (*fn)(struct ntdb_context *, void *), void *p)
{
	struct ntdb_context *i;

	for (i = tdbs; i; i = i->next) {
		if (fn(i, p) != 0)
			break;
	}
}
