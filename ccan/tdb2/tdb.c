#include "private.h"
#include <ccan/tdb2/tdb2.h>
#include <ccan/hash/hash.h>
#include <ccan/build_assert/build_assert.h>
#include <ccan/likely/likely.h>
#include <assert.h>

/* The null return. */
struct tdb_data tdb_null = { .dptr = NULL, .dsize = 0 };

/* all contexts, to ensure no double-opens (fcntl locks don't nest!) */
static struct tdb_context *tdbs = NULL;

PRINTF_ATTRIBUTE(4, 5) static void
null_log_fn(struct tdb_context *tdb,
	    enum tdb_debug_level level, void *priv,
	    const char *fmt, ...)
{
}

/* We do a lot of work assuming our copy of the header volatile area
 * is uptodate, and usually it is.  However, once we grab a lock, we have to
 * re-check it. */
bool update_header(struct tdb_context *tdb)
{
	struct tdb_header_volatile pad, *v;

	if (tdb->header_uptodate) {
		tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
			 "warning: header uptodate already\n");
	}

	/* We could get a partial update if we're not holding any locks. */
	assert(tdb_has_locks(tdb));

	v = tdb_get(tdb, offsetof(struct tdb_header, v), &pad, sizeof(*v));
	if (!v) {
		/* On failure, imply we updated header so they retry. */
		return true;
	}
	tdb->header_uptodate = true;
	if (likely(memcmp(&tdb->header.v, v, sizeof(*v)) == 0)) {
		return false;
	}
	tdb->header.v = *v;
	return true;
}

static uint64_t jenkins_hash(const void *key, size_t length, uint64_t seed,
			     void *arg)
{
	return hash64_stable((const unsigned char *)key, length, seed);
}

uint64_t tdb_hash(struct tdb_context *tdb, const void *ptr, size_t len)
{
	return tdb->khash(ptr, len, tdb->header.hash_seed, tdb->hash_priv);
}

static bool tdb_already_open(dev_t device, ino_t ino)
{
	struct tdb_context *i;
	
	for (i = tdbs; i; i = i->next) {
		if (i->device == device && i->inode == ino) {
			return true;
		}
	}

	return false;
}

static uint64_t random_number(struct tdb_context *tdb)
{
	int fd;
	uint64_t ret = 0;
	struct timeval now;

	fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		if (tdb_read_all(fd, &ret, sizeof(ret))) {
			tdb->log(tdb, TDB_DEBUG_TRACE, tdb->log_priv,
				 "tdb_open: random from /dev/urandom\n");
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
				tdb->log(tdb, TDB_DEBUG_TRACE, tdb->log_priv,
					 "tdb_open: %u random bytes from"
					 " /dev/egd-pool\n", r-1);
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
	tdb->log(tdb, TDB_DEBUG_TRACE, tdb->log_priv,
		 "tdb_open: random from getpid and time\n");
	return ret;
}

struct new_database {
	struct tdb_header hdr;
	struct tdb_used_record hrec;
	tdb_off_t hash[1ULL << INITIAL_HASH_BITS];
	struct tdb_used_record frec;
	tdb_off_t free[INITIAL_FREE_BUCKETS + 1]; /* One overflow bucket */
};

/* initialise a new database */
static int tdb_new_database(struct tdb_context *tdb)
{
	/* We make it up in memory, then write it out if not internal */
	struct new_database newdb;
	unsigned int magic_off = offsetof(struct tdb_header, magic_food);

	/* Fill in the header */
	newdb.hdr.version = TDB_VERSION;
	newdb.hdr.hash_seed = random_number(tdb);
	newdb.hdr.hash_test = TDB_HASH_MAGIC;
	newdb.hdr.hash_test = tdb->khash(&newdb.hdr.hash_test,
					 sizeof(newdb.hdr.hash_test),
					 newdb.hdr.hash_seed,
					 tdb->hash_priv);

	newdb.hdr.v.generation = 0;

	/* The initial zone must cover the initial database size! */
	BUILD_ASSERT((1ULL << INITIAL_ZONE_BITS) >= sizeof(newdb));

	/* Free array has 1 zone, 10 buckets.  All buckets empty. */
	newdb.hdr.v.num_zones = 1;
	newdb.hdr.v.zone_bits = INITIAL_ZONE_BITS;
	newdb.hdr.v.free_buckets = INITIAL_FREE_BUCKETS;
	newdb.hdr.v.free_off = offsetof(struct new_database, free);
	set_header(tdb, &newdb.frec, 0,
		   sizeof(newdb.free), sizeof(newdb.free), 0);
	memset(newdb.free, 0, sizeof(newdb.free));

	/* Initial hashes are empty. */
	newdb.hdr.v.hash_bits = INITIAL_HASH_BITS;
	newdb.hdr.v.hash_off = offsetof(struct new_database, hash);
	set_header(tdb, &newdb.hrec, 0,
		   sizeof(newdb.hash), sizeof(newdb.hash), 0);
	memset(newdb.hash, 0, sizeof(newdb.hash));

	/* Magic food */
	memset(newdb.hdr.magic_food, 0, sizeof(newdb.hdr.magic_food));
	strcpy(newdb.hdr.magic_food, TDB_MAGIC_FOOD);

	/* This creates an endian-converted database, as if read from disk */
	tdb_convert(tdb,
		    (char *)&newdb.hdr + magic_off,
		    sizeof(newdb) - magic_off);

	tdb->header = newdb.hdr;

	if (tdb->flags & TDB_INTERNAL) {
		tdb->map_size = sizeof(newdb);
		tdb->map_ptr = malloc(tdb->map_size);
		if (!tdb->map_ptr) {
			tdb->ecode = TDB_ERR_OOM;
			return -1;
		}
		memcpy(tdb->map_ptr, &newdb, tdb->map_size);
		return 0;
	}
	if (lseek(tdb->fd, 0, SEEK_SET) == -1)
		return -1;

	if (ftruncate(tdb->fd, 0) == -1)
		return -1;

	if (!tdb_pwrite_all(tdb->fd, &newdb, sizeof(newdb), 0)) {
		tdb->ecode = TDB_ERR_IO;
		return -1;
	}
	return 0;
}

struct tdb_context *tdb_open(const char *name, int tdb_flags,
			     int open_flags, mode_t mode,
			     union tdb_attribute *attr)
{
	struct tdb_context *tdb;
	struct stat st;
	int save_errno;
	uint64_t hash_test;
	unsigned v;

	if (!(tdb = (struct tdb_context *)calloc(1, sizeof *tdb))) {
		/* Can't log this */
		errno = ENOMEM;
		goto fail;
	}
	tdb->fd = -1;
	tdb->name = NULL;
	tdb->map_ptr = NULL;
	tdb->flags = tdb_flags;
	tdb->log = null_log_fn;
	tdb->log_priv = NULL;
	tdb->khash = jenkins_hash;
	tdb->hash_priv = NULL;
	tdb_io_init(tdb);

	/* FIXME */
	if (attr) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_open: attributes not yet supported\n");
		errno = EINVAL;
		goto fail;
	}

	if ((open_flags & O_ACCMODE) == O_WRONLY) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_open: can't open tdb %s write-only\n", name);
		errno = EINVAL;
		goto fail;
	}

	if ((open_flags & O_ACCMODE) == O_RDONLY) {
		tdb->read_only = 1;
		/* read only databases don't do locking */
		tdb->flags |= TDB_NOLOCK;
	}

	/* internal databases don't mmap or lock */
	if (tdb->flags & TDB_INTERNAL) {
		tdb->flags |= (TDB_NOLOCK | TDB_NOMMAP);
		if (tdb_new_database(tdb) != 0) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_open: tdb_new_database failed!");
			goto fail;
		}
		TEST_IT(tdb->flags & TDB_CONVERT);
		tdb_convert(tdb, &tdb->header, sizeof(tdb->header));
		goto internal;
	}

	if ((tdb->fd = open(name, open_flags, mode)) == -1) {
		tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
			 "tdb_open: could not open file %s: %s\n",
			 name, strerror(errno));
		goto fail;	/* errno set by open(2) */
	}

	/* on exec, don't inherit the fd */
	v = fcntl(tdb->fd, F_GETFD, 0);
        fcntl(tdb->fd, F_SETFD, v | FD_CLOEXEC);

	/* ensure there is only one process initialising at once */
	if (tdb_lock_open(tdb) == -1) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_open: failed to get open lock on %s: %s\n",
			 name, strerror(errno));
		goto fail;	/* errno set by tdb_brlock */
	}

	if (!tdb_pread_all(tdb->fd, &tdb->header, sizeof(tdb->header), 0)
	    || strcmp(tdb->header.magic_food, TDB_MAGIC_FOOD) != 0) {
		if (!(open_flags & O_CREAT) || tdb_new_database(tdb) == -1) {
			if (errno == 0) {
				errno = EIO; /* ie bad format or something */
			}
			goto fail;
		}
	} else if (tdb->header.version != TDB_VERSION) {
		if (tdb->header.version == bswap_64(TDB_VERSION))
			tdb->flags |= TDB_CONVERT;
		else {
			/* wrong version */
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_open: %s is unknown version 0x%llx\n",
				 name, (long long)tdb->header.version);
			errno = EIO;
			goto fail;
		}
	}

	tdb_convert(tdb, &tdb->header, sizeof(tdb->header));
	hash_test = TDB_HASH_MAGIC;
	hash_test = tdb->khash(&hash_test, sizeof(hash_test),
			       tdb->header.hash_seed, tdb->hash_priv);
	if (tdb->header.hash_test != hash_test) {
		/* wrong hash variant */
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_open: %s uses a different hash function\n",
			 name);
		errno = EIO;
		goto fail;
	}

	if (fstat(tdb->fd, &st) == -1)
		goto fail;

	/* Is it already in the open list?  If so, fail. */
	if (tdb_already_open(st.st_dev, st.st_ino)) {
		/* FIXME */
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_open: %s (%d,%d) is already open in this process\n",
			 name, (int)st.st_dev, (int)st.st_ino);
		errno = EBUSY;
		goto fail;
	}

	tdb->name = strdup(name);
	if (!tdb->name) {
		errno = ENOMEM;
		goto fail;
	}

	tdb->map_size = st.st_size;
	tdb->device = st.st_dev;
	tdb->inode = st.st_ino;
	tdb_io_init(tdb);
	tdb_mmap(tdb);

 internal:
	/* Internal (memory-only) databases skip all the code above to
	 * do with disk files, and resume here by releasing their
	 * open lock and hooking into the active list. */
	tdb_unlock_open(tdb);
	tdb->last_zone = random_free_zone(tdb);
	tdb->next = tdbs;
	tdbs = tdb;
	return tdb;

 fail:
	save_errno = errno;

	if (!tdb)
		return NULL;

#ifdef TDB_TRACE
	close(tdb->tracefd);
#endif
	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL) {
			free(tdb->map_ptr);
		} else
			tdb_munmap(tdb);
	}
	free((char *)tdb->name);
	if (tdb->fd != -1)
		if (close(tdb->fd) != 0)
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_open: failed to close tdb->fd"
				 " on error!\n");
	free(tdb);
	errno = save_errno;
	return NULL;
}

static int tdb_key_compare(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return memcmp(data.dptr, key.dptr, data.dsize) == 0;
}

static void unlock_lists(struct tdb_context *tdb,
			 uint64_t start, uint64_t end, int ltype)
{
	do {
		tdb_unlock_list(tdb, start, ltype);
		start = (start + ((1ULL << tdb->header.v.hash_bits) - 1))
			& ((1ULL << tdb->header.v.hash_bits) - 1);
	} while (start != end);
}

/* FIXME: Return header copy? */
/* Returns -1 or offset of entry (0 if not found).
 * Locks hash entried from *start to *end (where the entry was found). */
static tdb_off_t find_bucket_and_lock(struct tdb_context *tdb,
				      const struct tdb_data *key,
				      uint64_t hash,
				      uint64_t *start,
				      uint64_t *end,
				      uint64_t *room,
				      int ltype)
{
	uint64_t hextra;
	tdb_off_t off;

	/* hash_bits might be out of date... */
again:
	*start = *end = hash & ((1ULL << tdb->header.v.hash_bits) - 1);
	hextra = hash >> tdb->header.v.hash_bits;

	/* FIXME: can we avoid locks for some fast paths? */
	if (tdb_lock_list(tdb, *end, ltype, TDB_LOCK_WAIT) == -1)
		return TDB_OFF_ERR;

	/* We only need to check this for first lock. */
	if (unlikely(update_header(tdb))) {
		tdb_unlock_list(tdb, *end, ltype);
		goto again;
	}

	while ((off = tdb_read_off(tdb, tdb->header.v.hash_off
				   + *end * sizeof(tdb_off_t)))
	       != TDB_OFF_ERR) {
		struct tdb_used_record pad, *r;
		uint64_t keylen, next;

		/* Didn't find it? */
		if (!off)
			return 0;

#if 0 /* FIXME: Check other bits. */
		unsigned int bits, bitmask, hoffextra;
		/* Bottom three bits show how many extra hash bits. */
		bits = (off & ((1 << TDB_EXTRA_HASHBITS_NUM) - 1)) + 1;
		bitmask = (1 << bits)-1;
		hoffextra = ((off >> TDB_EXTRA_HASHBITS_NUM) & bitmask);
		if ((hextra & bitmask) != hoffextra) 
			goto lock_next;
#endif

		r = tdb_get(tdb, off, &pad, sizeof(*r));
		if (!r)
			goto unlock_err;

		if (rec_magic(r) != TDB_MAGIC) {
			tdb->ecode = TDB_ERR_CORRUPT;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "find_bucket_and_lock: bad magic 0x%llx"
				 " at offset %llu!\n",
				 (long long)rec_magic(r), (long long)off);
			goto unlock_err;
		}

		/* FIXME: check extra bits in header! */
		keylen = rec_key_length(r);
		if (keylen != key->dsize)
			goto lock_next;

		switch (tdb_parse_data(tdb, *key, off + sizeof(*r), key->dsize,
				       tdb_key_compare, NULL)) {
		case 1:
			/* Match! */
			*room = rec_data_length(r) + rec_extra_padding(r);
			return off >> TDB_EXTRA_HASHBITS_NUM;
		case 0:
			break;
		default:
			goto unlock_err;
		}

	lock_next:
		/* Lock next bucket. */
		/* FIXME: We can deadlock if this wraps! */
		next = (*end + 1) & ((1ULL << tdb->header.v.hash_bits) - 1);
		if (next == *start) {
			tdb->ecode = TDB_ERR_CORRUPT;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "find_bucket_and_lock: full hash table!\n");
			goto unlock_err;
		}
		if (tdb_lock_list(tdb, next, ltype, TDB_LOCK_WAIT) == -1)
			goto unlock_err;
		*end = next;
	}

unlock_err:
	TEST_IT(*end < *start);
	unlock_lists(tdb, *start, *end, ltype);
	return TDB_OFF_ERR;
}

static int update_rec_hdr(struct tdb_context *tdb,
			  tdb_off_t off,
			  tdb_len_t keylen,
			  tdb_len_t datalen,
			  tdb_len_t room,
			  uint64_t h)
{
	struct tdb_used_record rec;

	if (set_header(tdb, &rec, keylen, datalen, room - datalen, h))
		return -1;

	return tdb_write_convert(tdb, off, &rec, sizeof(rec));
}

/* If we fail, others will try after us. */
static void enlarge_hash(struct tdb_context *tdb)
{
	tdb_off_t newoff, i;
	uint64_t h, num = 1ULL << tdb->header.v.hash_bits;
	struct tdb_used_record pad, *r;

	/* FIXME: We should do this without holding locks throughout. */
	if (tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false) == -1)
		return;

	if (unlikely(update_header(tdb))) {
		/* Someone else enlarged for us?  Nothing to do. */
		if ((1ULL << tdb->header.v.hash_bits) != num)
			goto unlock;
	}

	newoff = alloc(tdb, 0, num * 2, 0, false);
	if (unlikely(newoff == TDB_OFF_ERR))
		goto unlock;
	if (unlikely(newoff == 0)) {
		if (tdb_expand(tdb, 0, num * 2, false) == -1)
			goto unlock;
		newoff = alloc(tdb, 0, num * 2, 0, false);
		if (newoff == TDB_OFF_ERR || newoff == 0)
			goto unlock;
	}

	/* FIXME: If the space before is empty, we know this is in its ideal
	 * location.  We can steal a bit from the pointer to avoid rehash. */
	for (i = tdb_find_nonzero_off(tdb, tdb->header.v.hash_off, num);
	     i < num;
	     i += tdb_find_nonzero_off(tdb, tdb->header.v.hash_off
				       + i*sizeof(tdb_off_t), num - i)) {
		tdb_off_t off;
		off = tdb_read_off(tdb, tdb->header.v.hash_off
				   + i*sizeof(tdb_off_t));
		if (unlikely(off == TDB_OFF_ERR))
			goto unlock;
		if (unlikely(!off)) {
			tdb->ecode = TDB_ERR_CORRUPT;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "find_bucket_and_lock: zero hash bucket!\n");
			goto unlock;
		}
		h = hash_record(tdb, off);
		/* FIXME: Encode extra hash bits! */
		if (tdb_write_off(tdb, newoff
				  + (h & ((num * 2) - 1)) * sizeof(uint64_t),
				  off) == -1)
			goto unlock;
	}

	/* Free up old hash. */
	r = tdb_get(tdb, tdb->header.v.hash_off, &pad, sizeof(*r));
	if (!r)
		goto unlock;
	add_free_record(tdb, tdb->header.v.hash_off,
			rec_data_length(r) + rec_extra_padding(r));

	/* Now we write the modified header. */
	tdb->header.v.generation++;
	tdb->header.v.hash_bits++;
	tdb->header.v.hash_off = newoff;
	tdb_write_convert(tdb, offsetof(struct tdb_header, v),
			  &tdb->header.v, sizeof(tdb->header.v));
unlock:
	tdb_allrecord_unlock(tdb, F_WRLCK);
}

int tdb_store(struct tdb_context *tdb,
	      struct tdb_data key, struct tdb_data dbuf, int flag)
{
	tdb_off_t new_off, off, start, end, room;
	uint64_t h;
	bool growing = false;

	h = tdb_hash(tdb, key.dptr, key.dsize);
	off = find_bucket_and_lock(tdb, &key, h, &start, &end, &room, F_WRLCK);
	if (off == TDB_OFF_ERR)
		return -1;

	/* Now we have lock on this hash bucket. */
	if (flag == TDB_INSERT) {
		if (off) {
			tdb->ecode = TDB_ERR_EXISTS;
			goto fail;
		}
	} else {
		if (off) {
			if (room >= key.dsize + dbuf.dsize) {
				new_off = off;
				if (update_rec_hdr(tdb, off,
						   key.dsize, dbuf.dsize,
						   room, h))
					goto fail;
				goto write;
			}
			/* FIXME: See if right record is free? */
			/* Hint to allocator that we've realloced. */
			growing = true;
		} else {
			if (flag == TDB_MODIFY) {
				/* if the record doesn't exist and we
				   are in TDB_MODIFY mode then we should fail
				   the store */
				tdb->ecode = TDB_ERR_NOEXIST;
				goto fail;
			}
		}
	}

	/* Allocate a new record. */
	new_off = alloc(tdb, key.dsize, dbuf.dsize, h, growing);
	if (new_off == 0) {
		unlock_lists(tdb, start, end, F_WRLCK);
		/* Expand, then try again... */
		if (tdb_expand(tdb, key.dsize, dbuf.dsize, growing) == -1)
			return -1;
		return tdb_store(tdb, key, dbuf, flag);
	}

	/* We didn't like the existing one: remove it. */
	if (off) {
		add_free_record(tdb, off, sizeof(struct tdb_used_record)
				+ key.dsize + room);
	}

write:
	off = tdb->header.v.hash_off + end * sizeof(tdb_off_t);
	/* FIXME: Encode extra hash bits! */
	if (tdb_write_off(tdb, off, new_off) == -1)
		goto fail;

	off = new_off + sizeof(struct tdb_used_record);
	if (tdb->methods->write(tdb, off, key.dptr, key.dsize) == -1)
		goto fail;
	off += key.dsize;
	if (tdb->methods->write(tdb, off, dbuf.dptr, dbuf.dsize) == -1)
		goto fail;

	/* FIXME: tdb_increment_seqnum(tdb); */
	unlock_lists(tdb, start, end, F_WRLCK);

	/* By simple trial and error, this roughly approximates a 60%
	 * full measure. */
	if (unlikely(end - start > 4 * tdb->header.v.hash_bits - 32))
		enlarge_hash(tdb);

	return 0;

fail:
	unlock_lists(tdb, start, end, F_WRLCK);
	return -1;
}

struct tdb_data tdb_fetch(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t off, start, end, room;
	uint64_t h;
	struct tdb_used_record pad, *r;
	struct tdb_data ret;

	h = tdb_hash(tdb, key.dptr, key.dsize);
	off = find_bucket_and_lock(tdb, &key, h, &start, &end, &room, F_RDLCK);
	if (off == TDB_OFF_ERR)
		return tdb_null;

	if (!off) {
		unlock_lists(tdb, start, end, F_RDLCK);
		tdb->ecode = TDB_SUCCESS;
		return tdb_null;
	}

	r = tdb_get(tdb, off, &pad, sizeof(*r));
	if (!r) {
		unlock_lists(tdb, start, end, F_RDLCK);
		return tdb_null;
	}

	ret.dsize = rec_data_length(r);
	ret.dptr = tdb_alloc_read(tdb, off + sizeof(*r) + key.dsize,
				  ret.dsize);
	unlock_lists(tdb, start, end, F_RDLCK);
	return ret;
}

static int hash_add(struct tdb_context *tdb, uint64_t h, tdb_off_t off)
{
	tdb_off_t i, hoff, len, num;

	i = (h & ((1ULL << tdb->header.v.hash_bits) - 1));
	hoff = tdb->header.v.hash_off + i * sizeof(tdb_off_t);
	len = (1ULL << tdb->header.v.hash_bits) - i;

	/* Look for next space. */
	num = tdb_find_zero_off(tdb, hoff, len);
	if (unlikely(num == len)) {
		hoff = tdb->header.v.hash_off;
		len = (1ULL << tdb->header.v.hash_bits);
		num = tdb_find_zero_off(tdb, hoff, len);
		if (i == len)
			return -1;
	}
	/* FIXME: Encode extra hash bits! */
	return tdb_write_off(tdb, hoff + num * sizeof(tdb_off_t), off);
}

static int unlink_used_record(struct tdb_context *tdb, tdb_off_t chain,
			      uint64_t *extra_locks)
{
	tdb_off_t num, len, i, hoff;

	/* FIXME: Maybe lock more in search?  Maybe don't lock if scan
	 * finds none? */
again:
	len = (1ULL << tdb->header.v.hash_bits) - (chain + 1);
	hoff = tdb->header.v.hash_off + (chain + 1) * sizeof(tdb_off_t);
	num = tdb_find_zero_off(tdb, hoff, len);

	/* We want to lock the zero entry, too.  In the wrap case,
	 * this locks one extra.  That's harmless. */
	num++;

	for (i = chain + 1; i < chain + 1 + num; i++) {
		if (tdb_lock_list(tdb, i, F_WRLCK, TDB_LOCK_WAIT) == -1) {
			if (i != chain + 1)
				unlock_lists(tdb, chain + 1, i-1, F_WRLCK);
			return -1;
		}
	}

	/* The wrap case: we need those locks out of order! */
	if (unlikely(num == len + 1)) {
		*extra_locks = tdb_find_zero_off(tdb, tdb->header.v.hash_off,
						 1ULL << tdb->header.v.hash_bits);
		(*extra_locks)++;
		for (i = 0; i < *extra_locks; i++) {
			if (tdb_lock_list(tdb, i, F_WRLCK, TDB_LOCK_NOWAIT)) {
				/* Failed.  Caller must lock in order. */
				if (i)
					unlock_lists(tdb, 0, i-1, F_WRLCK);
				unlock_lists(tdb, chain + 1, chain + num,
					     F_WRLCK);
				return 1;
			}
		}
		num += *extra_locks;
	}

	/* Now we have the locks, be certain that offset is still 0! */
	hoff = tdb->header.v.hash_off
		+ (((chain + num) * sizeof(tdb_off_t))
		   & ((1ULL << tdb->header.v.hash_bits) - 1));

	if (unlikely(tdb_read_off(tdb, hoff) != 0)) {
		unlock_lists(tdb, chain + 1, chain + num, F_WRLCK);
		goto again;
	}

	/* OK, all locked.  Unlink first one. */
	hoff = tdb->header.v.hash_off + chain * sizeof(tdb_off_t);
	if (tdb_write_off(tdb, hoff, 0) == -1)
		goto unlock_err;

	/* Rehash the rest. */
	for (i = 1; i < num; i++) {
		tdb_off_t off;
		uint64_t h;

		hoff = tdb->header.v.hash_off
			+ (((chain + i) * sizeof(tdb_off_t))
			   & ((1ULL << tdb->header.v.hash_bits) - 1));
		off = tdb_read_off(tdb, hoff);
		if (unlikely(off == TDB_OFF_ERR))
			goto unlock_err;

		/* Maybe use a bit to indicate it is in ideal place? */
		h = hash_record(tdb, off);
		/* Is it happy where it is? */
		if ((h & ((1ULL << tdb->header.v.hash_bits)-1)) == (chain + i))
			continue;

		/* Remove it. */
		if (tdb_write_off(tdb, hoff, 0) == -1)
			goto unlock_err;

		/* Rehash it. */
		if (hash_add(tdb, h, off) == -1)
			goto unlock_err;
	}
	unlock_lists(tdb, chain + 1, chain + num, F_WRLCK);
	return 0;

unlock_err:
	unlock_lists(tdb, chain + 1, chain + num, F_WRLCK);
	return -1;
}

int tdb_delete(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t off, start, end, room, extra_locks = 0;
	uint64_t h;
	int ret;

	h = tdb_hash(tdb, key.dptr, key.dsize);
	off = find_bucket_and_lock(tdb, &key, h, &start, &end, &room, F_WRLCK);
	if (off == TDB_OFF_ERR)
		return -1;

	if (off == 0) {
		unlock_lists(tdb, start, end, F_WRLCK);
		tdb->ecode = TDB_ERR_NOEXIST;
		return -1;
	}

	ret = unlink_used_record(tdb, end, &extra_locks);
	if (unlikely(ret == 1)) {
		unsigned int i;

		unlock_lists(tdb, start, end, F_WRLCK);

		/* We need extra locks at the start. */
		for (i = 0; i < extra_locks; i++) {
			if (tdb_lock_list(tdb, i, F_WRLCK, TDB_LOCK_WAIT)) {
				if (i)
					unlock_lists(tdb, 0, i-1, F_WRLCK);
				return -1;
			}
		}
		/* Try again now we're holding more locks. */
		ret = tdb_delete(tdb, key);
		unlock_lists(tdb, 0, i, F_WRLCK);
		return ret;
	}
	unlock_lists(tdb, start, end, F_WRLCK);
	return ret;
}

int tdb_close(struct tdb_context *tdb)
{
	struct tdb_context **i;
	int ret = 0;

	/* FIXME:
	if (tdb->transaction) {
		tdb_transaction_cancel(tdb);
	}
	*/
	tdb_trace(tdb, "tdb_close");

	if (tdb->map_ptr) {
		if (tdb->flags & TDB_INTERNAL)
			free(tdb->map_ptr);
		else
			tdb_munmap(tdb);
	}
	free((char *)tdb->name);
	if (tdb->fd != -1) {
		ret = close(tdb->fd);
		tdb->fd = -1;
	}
	free(tdb->lockrecs);

	/* Remove from contexts list */
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
