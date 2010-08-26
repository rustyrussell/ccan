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
bool header_changed(struct tdb_context *tdb)
{
	uint64_t gen;

	if (!(tdb->flags & TDB_NOLOCK) && tdb->header_uptodate) {
		tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
			 "warning: header uptodate already\n");
	}

	/* We could get a partial update if we're not holding any locks. */
	assert((tdb->flags & TDB_NOLOCK) || tdb_has_locks(tdb));

	tdb->header_uptodate = true;
	gen = tdb_read_off(tdb, offsetof(struct tdb_header, v.generation));
	if (unlikely(gen != tdb->header.v.generation)) {
		tdb_read_convert(tdb, offsetof(struct tdb_header, v),
				 &tdb->header.v, sizeof(tdb->header.v));
		return true;
	}
	return false;
}

int write_header(struct tdb_context *tdb)
{
	assert(tdb_read_off(tdb, offsetof(struct tdb_header, v.generation))
	       == tdb->header.v.generation);
	tdb->header.v.generation++;
	return tdb_write_convert(tdb, offsetof(struct tdb_header, v),
				 &tdb->header.v, sizeof(tdb->header.v));
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

	tdb = malloc(sizeof(*tdb));
	if (!tdb) {
		/* Can't log this */
		errno = ENOMEM;
		goto fail;
	}
	tdb->name = NULL;
	tdb->map_ptr = NULL;
	tdb->fd = -1;
	/* map_size will be set below. */
	tdb->ecode = TDB_SUCCESS;
	/* header will be read in below. */
	tdb->header_uptodate = false;
	tdb->flags = tdb_flags;
	tdb->log = null_log_fn;
	tdb->log_priv = NULL;
	tdb->khash = jenkins_hash;
	tdb->hash_priv = NULL;
	tdb->transaction = NULL;
	/* last_zone will be set below. */
	tdb_io_init(tdb);
	tdb_lock_init(tdb);

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
		tdb->read_only = true;
		/* read only databases don't do locking */
		tdb->flags |= TDB_NOLOCK;
	} else
		tdb->read_only = false;

	/* internal databases don't need any of the rest. */
	if (tdb->flags & TDB_INTERNAL) {
		tdb->flags |= (TDB_NOLOCK | TDB_NOMMAP);
		if (tdb_new_database(tdb) != 0) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_open: tdb_new_database failed!");
			goto fail;
		}
		TEST_IT(tdb->flags & TDB_CONVERT);
		tdb_convert(tdb, &tdb->header, sizeof(tdb->header));
		return tdb;
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
	tdb_mmap(tdb);
	tdb_unlock_open(tdb);
	tdb_zone_init(tdb);

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

static tdb_off_t hash_off(struct tdb_context *tdb, uint64_t list)
{
	return tdb->header.v.hash_off
		+ ((list & ((1ULL << tdb->header.v.hash_bits) - 1))
		   * sizeof(tdb_off_t));
}

/* Returns 0 if the entry is a zero (definitely not a match).
 * Returns a valid entry offset if it's a match.  Fills in rec.
 * Otherwise returns TDB_OFF_ERR: keep searching. */
static tdb_off_t entry_matches(struct tdb_context *tdb,
			       uint64_t list,
			       uint64_t hash,
			       const struct tdb_data *key,
			       struct tdb_used_record *rec)
{
	tdb_off_t off;
	uint64_t keylen;
	const unsigned char *rkey;

	off = tdb_read_off(tdb, tdb->header.v.hash_off
			   + list * sizeof(tdb_off_t));
	if (off == 0 || off == TDB_OFF_ERR)
		return off;

#if 0 /* FIXME: Check other bits. */
	unsigned int bits, bitmask, hoffextra;
	/* Bottom three bits show how many extra hash bits. */
	bits = (off & ((1 << TDB_EXTRA_HASHBITS_NUM) - 1)) + 1;
	bitmask = (1 << bits)-1;
	hoffextra = ((off >> TDB_EXTRA_HASHBITS_NUM) & bitmask);
	uint64_t hextra = hash >> tdb->header.v.hash_bits;
	if ((hextra & bitmask) != hoffextra) 
		return TDB_OFF_ERR;
	off &= ~...;
#endif

	if (tdb_read_convert(tdb, off, rec, sizeof(*rec)) == -1)
		return TDB_OFF_ERR;

	/* FIXME: check extra bits in header! */
	keylen = rec_key_length(rec);
	if (keylen != key->dsize)
		return TDB_OFF_ERR;

	rkey = tdb_access_read(tdb, off + sizeof(*rec), keylen, false);
	if (!rkey)
		return TDB_OFF_ERR;
	if (memcmp(rkey, key->dptr, keylen) != 0)
		off = TDB_OFF_ERR;
	tdb_access_release(tdb, rkey);
	return off;
}

/* FIXME: Optimize? */
static void unlock_lists(struct tdb_context *tdb,
			 tdb_off_t list, tdb_len_t num,
			 int ltype)
{
	tdb_off_t i;

	for (i = list; i < list + num; i++)
		tdb_unlock_list(tdb, i, ltype);
}

/* FIXME: Optimize? */
static int lock_lists(struct tdb_context *tdb,
		      tdb_off_t list, tdb_len_t num,
		      int ltype)
{
	tdb_off_t i;

	for (i = list; i < list + num; i++) {
		if (tdb_lock_list(tdb, i, ltype, TDB_LOCK_WAIT) != 0) {
			unlock_lists(tdb, list, i - list, ltype);
			return -1;
		}
	}
	return 0;
}

/* We lock hashes up to the next empty offset.  We already hold the
 * lock on the start bucket, but we may need to release and re-grab
 * it.  If we fail, we hold no locks at all! */
static tdb_len_t relock_hash_to_zero(struct tdb_context *tdb,
				     tdb_off_t start, int ltype)
{
	tdb_len_t num, len, pre_locks;

again:
	num = 1ULL << tdb->header.v.hash_bits;
	len = tdb_find_zero_off(tdb, hash_off(tdb, start), num - start);
	if (unlikely(len == num - start)) {
		/* We hit the end of the hash range.  Drop lock: we have
		   to lock start of hash first. */
		tdb_unlock_list(tdb, start, ltype);
		/* Grab something, so header is stable. */
		if (tdb_lock_list(tdb, 0, ltype, TDB_LOCK_WAIT))
			return TDB_OFF_ERR;
		len = tdb_find_zero_off(tdb, hash_off(tdb, 0), num);
		if (lock_lists(tdb, 1, len, ltype) == -1) {
			tdb_unlock_list(tdb, 0, ltype);
			return TDB_OFF_ERR;
		}
		pre_locks = len;
		len = num - start;
	} else {
		/* We already have lock on start. */
		start++;
		pre_locks = 0;
	}
	if (unlikely(lock_lists(tdb, start, len, ltype) == -1)) {
		if (pre_locks)
			unlock_lists(tdb, 0, pre_locks, ltype);
		else
			tdb_unlock_list(tdb, start, ltype);
		return TDB_OFF_ERR;
	}

	/* Now, did we lose the race, and it's not zero any more? */
	if (unlikely(tdb_read_off(tdb, hash_off(tdb, pre_locks + len)) != 0)) {
		unlock_lists(tdb, 0, pre_locks, ltype);
		/* Leave the start locked, as expected. */
		unlock_lists(tdb, start + 1, len - 1, ltype);
		goto again;
	}

	return pre_locks + len;
}

/* FIXME: modify, don't rewrite! */
static int update_rec_hdr(struct tdb_context *tdb,
			  tdb_off_t off,
			  tdb_len_t keylen,
			  tdb_len_t datalen,
			  struct tdb_used_record *rec,
			  uint64_t h)
{
	uint64_t room = rec_data_length(rec) + rec_extra_padding(rec);

	if (set_header(tdb, rec, keylen, datalen, room - datalen, h))
		return -1;

	return tdb_write_convert(tdb, off, rec, sizeof(*rec));
}

/* If we fail, others will try after us. */
static void enlarge_hash(struct tdb_context *tdb)
{
	tdb_off_t newoff, oldoff, i;
	tdb_len_t hlen;
	uint64_t h, num = 1ULL << tdb->header.v.hash_bits;
	struct tdb_used_record pad, *r;

	/* FIXME: We should do this without holding locks throughout. */
	if (tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false) == -1)
		return;

	/* Someone else enlarged for us?  Nothing to do. */
	if ((1ULL << tdb->header.v.hash_bits) != num)
		goto unlock;

	/* Allocate our new array. */
	hlen = num * sizeof(tdb_off_t) * 2;
	newoff = alloc(tdb, 0, hlen, 0, false);
	if (unlikely(newoff == TDB_OFF_ERR))
		goto unlock;
	if (unlikely(newoff == 0)) {
		if (tdb_expand(tdb, 0, hlen, false) == -1)
			goto unlock;
		newoff = alloc(tdb, 0, hlen, 0, false);
		if (newoff == TDB_OFF_ERR || newoff == 0)
			goto unlock;
	}
	/* Step over record header! */
	newoff += sizeof(struct tdb_used_record);

	/* Starts all zero. */
	if (zero_out(tdb, newoff, hlen) == -1)
		goto unlock;

	/* FIXME: If the space before is empty, we know this is in its ideal
	 * location.  Or steal a bit from the pointer to avoid rehash. */
	for (i = tdb_find_nonzero_off(tdb, hash_off(tdb, 0), num);
	     i < num;
	     i += tdb_find_nonzero_off(tdb, hash_off(tdb, i), num - i)) {
		tdb_off_t off;
		off = tdb_read_off(tdb, hash_off(tdb, i));
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
	oldoff = tdb->header.v.hash_off - sizeof(*r);
	r = tdb_get(tdb, oldoff, &pad, sizeof(*r));
	if (!r)
		goto unlock;
	add_free_record(tdb, oldoff,
			sizeof(*r)+rec_data_length(r)+rec_extra_padding(r));

	/* Now we write the modified header. */
	tdb->header.v.hash_bits++;
	tdb->header.v.hash_off = newoff;
	write_header(tdb);
unlock:
	tdb_allrecord_unlock(tdb, F_WRLCK);
}

int tdb_store(struct tdb_context *tdb,
	      struct tdb_data key, struct tdb_data dbuf, int flag)
{
	tdb_off_t new_off, off, old_bucket, start, num_locks = 1;
	struct tdb_used_record rec;
	uint64_t h;
	bool growing = false;

	h = tdb_hash(tdb, key.dptr, key.dsize);

	/* FIXME: can we avoid locks for some fast paths? */
	start = tdb_lock_list(tdb, h, F_WRLCK, TDB_LOCK_WAIT);
	if (start == TDB_OFF_ERR)
		return -1;

	/* Fast path. */
	old_bucket = start;
	off = entry_matches(tdb, start, h, &key, &rec);
	if (unlikely(off == TDB_OFF_ERR)) {
		/* Slow path, need to grab more locks and search. */
		tdb_off_t i;

		/* Warning: this may drop the lock!  Does that on error. */
		num_locks = relock_hash_to_zero(tdb, start, F_WRLCK);
		if (num_locks == TDB_OFF_ERR)
			return -1;

		for (i = start; i < start + num_locks; i++) {
			off = entry_matches(tdb, i, h, &key, &rec);
			/* Empty entry or we found it? */
			if (off == 0 || off != TDB_OFF_ERR) {
				old_bucket = i;
				break;
			}
		}
		if (i == start + num_locks)
			off = 0;
	}

	/* Now we have lock on this hash bucket. */
	if (flag == TDB_INSERT) {
		if (off) {
			tdb->ecode = TDB_ERR_EXISTS;
			goto fail;
		}
	} else {
		if (off) {
			if (rec_data_length(&rec) + rec_extra_padding(&rec)
			    >= dbuf.dsize) {
				new_off = off;
				if (update_rec_hdr(tdb, off,
						   key.dsize, dbuf.dsize,
						   &rec, h))
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
		unlock_lists(tdb, start, num_locks, F_WRLCK);
		/* Expand, then try again... */
		if (tdb_expand(tdb, key.dsize, dbuf.dsize, growing) == -1)
			return -1;
		return tdb_store(tdb, key, dbuf, flag);
	}

	/* We didn't like the existing one: remove it. */
	if (off) {
		add_free_record(tdb, off, sizeof(struct tdb_used_record)
				+ rec_key_length(&rec)
				+ rec_data_length(&rec)
				+ rec_extra_padding(&rec));
	}

write:
	/* FIXME: Encode extra hash bits! */
	if (tdb_write_off(tdb, hash_off(tdb, old_bucket), new_off) == -1)
		goto fail;

	off = new_off + sizeof(struct tdb_used_record);
	if (tdb->methods->write(tdb, off, key.dptr, key.dsize) == -1)
		goto fail;
	off += key.dsize;
	if (tdb->methods->write(tdb, off, dbuf.dptr, dbuf.dsize) == -1)
		goto fail;

	/* FIXME: tdb_increment_seqnum(tdb); */
	unlock_lists(tdb, start, num_locks, F_WRLCK);

	/* FIXME: by simple simulation, this approximated 60% full.
	 * Check in real case! */
	if (unlikely(num_locks > 4 * tdb->header.v.hash_bits - 31))
		enlarge_hash(tdb);

	return 0;

fail:
	unlock_lists(tdb, start, num_locks, F_WRLCK);
	return -1;
}

struct tdb_data tdb_fetch(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t off, start, num_locks = 1;
	struct tdb_used_record rec;
	uint64_t h;
	struct tdb_data ret;

	h = tdb_hash(tdb, key.dptr, key.dsize);

	/* FIXME: can we avoid locks for some fast paths? */
	start = tdb_lock_list(tdb, h, F_RDLCK, TDB_LOCK_WAIT);
	if (start == TDB_OFF_ERR)
		return tdb_null;

	/* Fast path. */
	off = entry_matches(tdb, start, h, &key, &rec);
	if (unlikely(off == TDB_OFF_ERR)) {
		/* Slow path, need to grab more locks and search. */
		tdb_off_t i;

		/* Warning: this may drop the lock!  Does that on error. */
		num_locks = relock_hash_to_zero(tdb, start, F_RDLCK);
		if (num_locks == TDB_OFF_ERR)
			return tdb_null;

		for (i = start; i < start + num_locks; i++) {
			off = entry_matches(tdb, i, h, &key, &rec);
			/* Empty entry or we found it? */
			if (off == 0 || off != TDB_OFF_ERR)
				break;
		}
		if (i == start + num_locks)
			off = 0;
	}

	if (!off) {
		unlock_lists(tdb, start, num_locks, F_RDLCK);
		tdb->ecode = TDB_ERR_NOEXIST;
		return tdb_null;
	}

	ret.dsize = rec_data_length(&rec);
	ret.dptr = tdb_alloc_read(tdb, off + sizeof(rec) + key.dsize,
				  ret.dsize);
	unlock_lists(tdb, start, num_locks, F_RDLCK);
	return ret;
}

static int hash_add(struct tdb_context *tdb, uint64_t h, tdb_off_t off)
{
	tdb_off_t i, hoff, len, num;

	/* Look for next space. */
	i = (h & ((1ULL << tdb->header.v.hash_bits) - 1));
	len = (1ULL << tdb->header.v.hash_bits) - i;
	num = tdb_find_zero_off(tdb, hash_off(tdb, i), len);

	if (unlikely(num == len)) {
		/* We wrapped.  Look through start of hash table. */
		hoff = hash_off(tdb, 0);
		len = (1ULL << tdb->header.v.hash_bits);
		num = tdb_find_zero_off(tdb, hoff, len);
		if (i == len) {
			tdb->ecode = TDB_ERR_CORRUPT;
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "hash_add: full hash table!\n");
			return -1;
		}
	}
	/* FIXME: Encode extra hash bits! */
	return tdb_write_off(tdb, hash_off(tdb, i + num), off);
}

int tdb_delete(struct tdb_context *tdb, struct tdb_data key)
{
	tdb_off_t i, old_bucket, off, start, num_locks = 1;
	struct tdb_used_record rec;
	uint64_t h;

	h = tdb_hash(tdb, key.dptr, key.dsize);

	/* FIXME: can we avoid locks for some fast paths? */
	start = tdb_lock_list(tdb, h, F_WRLCK, TDB_LOCK_WAIT);
	if (start == TDB_OFF_ERR)
		return -1;

	/* Fast path. */
	old_bucket = start;
	off = entry_matches(tdb, start, h, &key, &rec);
	if (off && off != TDB_OFF_ERR) {
		/* We can only really fastpath delete if next bucket
		 * is 0.  Note that we haven't locked it, but our lock
		 * on this bucket stops anyone overflowing into it
		 * while we look. */
		if (tdb_read_off(tdb, hash_off(tdb, h+1)) == 0)
			goto delete;
		/* Slow path. */
		off = TDB_OFF_ERR;
	}

	if (unlikely(off == TDB_OFF_ERR)) {
		/* Slow path, need to grab more locks and search. */
		tdb_off_t i;

		/* Warning: this may drop the lock!  Does that on error. */
		num_locks = relock_hash_to_zero(tdb, start, F_WRLCK);
		if (num_locks == TDB_OFF_ERR)
			return -1;

		for (i = start; i < start + num_locks; i++) {
			off = entry_matches(tdb, i, h, &key, &rec);
			/* Empty entry or we found it? */
			if (off == 0 || off != TDB_OFF_ERR) {
				old_bucket = i;
				break;
			}
		}
		if (i == start + num_locks)
			off = 0;
	}

	if (!off) {
		unlock_lists(tdb, start, num_locks, F_WRLCK);
		tdb->ecode = TDB_ERR_NOEXIST;
		return -1;
	}

delete:
	/* This actually unlinks it. */
	if (tdb_write_off(tdb, hash_off(tdb, old_bucket), 0) == -1)
		goto unlock_err;

	/* Rehash anything following. */
	for (i = hash_off(tdb, old_bucket+1);
	     i != hash_off(tdb, h + num_locks);
	     i += sizeof(tdb_off_t)) {
		tdb_off_t off2;
		uint64_t h2;

		off2 = tdb_read_off(tdb, i);
		if (unlikely(off2 == TDB_OFF_ERR))
			goto unlock_err;

		/* Maybe use a bit to indicate it is in ideal place? */
		h2 = hash_record(tdb, off2);
		/* Is it happy where it is? */
		if (hash_off(tdb, h2) == i)
			continue;

		/* Remove it. */
		if (tdb_write_off(tdb, i, 0) == -1)
			goto unlock_err;

		/* Rehash it. */
		if (hash_add(tdb, h2, off2) == -1)
			goto unlock_err;
	}

	/* Free the deleted entry. */
	if (add_free_record(tdb, off,
			    sizeof(struct tdb_used_record)
			    + rec_key_length(&rec)
			    + rec_data_length(&rec)
			    + rec_extra_padding(&rec)) != 0)
		goto unlock_err;

	unlock_lists(tdb, start, num_locks, F_WRLCK);
	return 0;

unlock_err:
	unlock_lists(tdb, start, num_locks, F_WRLCK);
	return -1;
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

enum TDB_ERROR tdb_error(struct tdb_context *tdb)
{
	return tdb->ecode;
}
