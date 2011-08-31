#ifndef TDB_PRIVATE_H
#define TDB_PRIVATE_H
 /*
   Trivial Database 2: private types and prototypes
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

#include <ccan/tdb2/tdb2.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <utime.h>
#include <unistd.h>
#include <ccan/likely/likely.h>
#include <ccan/endian/endian.h>

#ifndef TEST_IT
#define TEST_IT(cond)
#endif

/* #define TDB_TRACE 1 */

#ifndef __STRING
#define __STRING(x)    #x
#endif

#ifndef __STRINGSTRING
#define __STRINGSTRING(x) __STRING(x)
#endif

#ifndef __location__
#define __location__ __FILE__ ":" __STRINGSTRING(__LINE__)
#endif

typedef uint64_t tdb_len_t;
typedef uint64_t tdb_off_t;

#define TDB_MAGIC_FOOD "TDB file\n"
#define TDB_VERSION ((uint64_t)(0x26011967 + 7))
#define TDB_USED_MAGIC ((uint64_t)0x1999)
#define TDB_HTABLE_MAGIC ((uint64_t)0x1888)
#define TDB_CHAIN_MAGIC ((uint64_t)0x1777)
#define TDB_FTABLE_MAGIC ((uint64_t)0x1666)
#define TDB_FREE_MAGIC ((uint64_t)0xFE)
#define TDB_HASH_MAGIC (0xA1ABE11A01092008ULL)
#define TDB_RECOVERY_MAGIC (0xf53bc0e7ad124589ULL)
#define TDB_RECOVERY_INVALID_MAGIC (0x0ULL)

#define TDB_OFF_IS_ERR(off) unlikely(off >= (tdb_off_t)TDB_ERR_LAST)

/* Packing errors into pointers and v.v. */
#define TDB_PTR_IS_ERR(ptr) \
	unlikely((unsigned long)(ptr) >= (unsigned long)TDB_ERR_LAST)
#define TDB_PTR_ERR(p) ((enum TDB_ERROR)(long)(p))
#define TDB_ERR_PTR(err) ((void *)(long)(err))

/* Common case of returning true, false or -ve error. */
typedef int tdb_bool_err;

/* Prevent others from opening the file. */
#define TDB_OPEN_LOCK 0
/* Expanding file. */
#define TDB_EXPANSION_LOCK 2
/* Doing a transaction. */
#define TDB_TRANSACTION_LOCK 8
/* Hash chain locks. */
#define TDB_HASH_LOCK_START 64

/* Range for hash locks. */
#define TDB_HASH_LOCK_RANGE_BITS 30
#define TDB_HASH_LOCK_RANGE (1 << TDB_HASH_LOCK_RANGE_BITS)

/* We have 1024 entries in the top level. */
#define TDB_TOPLEVEL_HASH_BITS 10
/* And 64 entries in each sub-level: thus 64 bits exactly after 9 levels. */
#define TDB_SUBLEVEL_HASH_BITS 6
/* And 8 entries in each group, ie 8 groups per sublevel. */
#define TDB_HASH_GROUP_BITS 3
/* This is currently 10: beyond this we chain. */
#define TDB_MAX_LEVELS (1+(64-TDB_TOPLEVEL_HASH_BITS) / TDB_SUBLEVEL_HASH_BITS)

/* Extend file by least 100 times larger than needed. */
#define TDB_EXTENSION_FACTOR 100

/* We steal bits from the offsets to store hash info. */
#define TDB_OFF_HASH_GROUP_MASK ((1ULL << TDB_HASH_GROUP_BITS) - 1)
/* We steal this many upper bits, giving a maximum offset of 64 exabytes. */
#define TDB_OFF_UPPER_STEAL 8
#define   TDB_OFF_UPPER_STEAL_EXTRA 7
/* The bit number where we store extra hash bits. */
#define TDB_OFF_HASH_EXTRA_BIT 57
#define TDB_OFF_UPPER_STEAL_SUBHASH_BIT 56

/* Additional features we understand.  Currently: none. */
#define TDB_FEATURE_MASK ((uint64_t)0)

/* The bit number where we store the extra hash bits. */
/* Convenience mask to get actual offset. */
#define TDB_OFF_MASK \
	(((1ULL << (64 - TDB_OFF_UPPER_STEAL)) - 1) - TDB_OFF_HASH_GROUP_MASK)

/* How many buckets in a free list: see size_to_bucket(). */
#define TDB_FREE_BUCKETS (64 - TDB_OFF_UPPER_STEAL)

/* We have to be able to fit a free record here. */
#define TDB_MIN_DATA_LEN	\
	(sizeof(struct tdb_free_record) - sizeof(struct tdb_used_record))

/* Indicates this entry is not on an flist (can happen during coalescing) */
#define TDB_FTABLE_NONE ((1ULL << TDB_OFF_UPPER_STEAL) - 1)

struct tdb_used_record {
	/* For on-disk compatibility, we avoid bitfields:
	   magic: 16,        (highest)
	   key_len_bits: 5,
	   extra_padding: 32
	   hash_bits: 11
	*/
        uint64_t magic_and_meta;
	/* The bottom key_len_bits*2 are key length, rest is data length. */
        uint64_t key_and_data_len;
};

static inline unsigned rec_key_bits(const struct tdb_used_record *r)
{
	return ((r->magic_and_meta >> 43) & ((1 << 5)-1)) * 2;
}

static inline uint64_t rec_key_length(const struct tdb_used_record *r)
{
	return r->key_and_data_len & ((1ULL << rec_key_bits(r)) - 1);
}

static inline uint64_t rec_data_length(const struct tdb_used_record *r)
{
	return r->key_and_data_len >> rec_key_bits(r);
}

static inline uint64_t rec_extra_padding(const struct tdb_used_record *r)
{
	return (r->magic_and_meta >> 11) & 0xFFFFFFFF;
}

static inline uint32_t rec_hash(const struct tdb_used_record *r)
{
	return r->magic_and_meta & ((1 << 11) - 1);
}

static inline uint16_t rec_magic(const struct tdb_used_record *r)
{
	return (r->magic_and_meta >> 48);
}

struct tdb_free_record {
        uint64_t magic_and_prev; /* TDB_OFF_UPPER_STEAL bits magic, then prev */
        uint64_t ftable_and_len; /* Len not counting these two fields. */
	/* This is why the minimum record size is 8 bytes.  */
	uint64_t next;
};

static inline uint64_t frec_prev(const struct tdb_free_record *f)
{
	return f->magic_and_prev & ((1ULL << (64 - TDB_OFF_UPPER_STEAL)) - 1);
}

static inline uint64_t frec_magic(const struct tdb_free_record *f)
{
	return f->magic_and_prev >> (64 - TDB_OFF_UPPER_STEAL);
}

static inline uint64_t frec_len(const struct tdb_free_record *f)
{
	return f->ftable_and_len & ((1ULL << (64 - TDB_OFF_UPPER_STEAL))-1);
}

static inline unsigned frec_ftable(const struct tdb_free_record *f)
{
	return f->ftable_and_len >> (64 - TDB_OFF_UPPER_STEAL);
}

struct tdb_recovery_record {
	uint64_t magic;
	/* Length of record (add this header to get total length). */
	uint64_t max_len;
	/* Length used. */
	uint64_t len;
	/* Old length of file before transaction. */
	uint64_t eof;
};

/* If we bottom out of the subhashes, we chain. */
struct tdb_chain {
	tdb_off_t rec[1 << TDB_HASH_GROUP_BITS];
	tdb_off_t next;
};

/* this is stored at the front of every database */
struct tdb_header {
	char magic_food[64]; /* for /etc/magic */
	/* FIXME: Make me 32 bit? */
	uint64_t version; /* version of the code */
	uint64_t hash_test; /* result of hashing HASH_MAGIC. */
	uint64_t hash_seed; /* "random" seed written at creation time. */
	tdb_off_t free_table; /* (First) free table. */
	tdb_off_t recovery; /* Transaction recovery area. */

	uint64_t features_used; /* Features all writers understand */
	uint64_t features_offered; /* Features offered */

	uint64_t seqnum; /* Sequence number for TDB_SEQNUM */

	tdb_off_t reserved[23];

	/* Top level hash table. */
	tdb_off_t hashtable[1ULL << TDB_TOPLEVEL_HASH_BITS];
};

struct tdb_freetable {
	struct tdb_used_record hdr;
	tdb_off_t next;
	tdb_off_t buckets[TDB_FREE_BUCKETS];
};

/* Information about a particular (locked) hash entry. */
struct hash_info {
	/* Full hash value of entry. */
	uint64_t h;
	/* Start and length of lock acquired. */
	tdb_off_t hlock_start;
	tdb_len_t hlock_range;
	/* Start of hash group. */
	tdb_off_t group_start;
	/* Bucket we belong in. */
	unsigned int home_bucket;
	/* Bucket we (or an empty space) were found in. */
	unsigned int found_bucket;
	/* How many bits of the hash are already used. */
	unsigned int hash_used;
	/* Current working group. */
	tdb_off_t group[1 << TDB_HASH_GROUP_BITS];
};

struct traverse_info {
	struct traverse_level {
		tdb_off_t hashtable;
		/* We ignore groups here, and treat it as a big array. */
		unsigned entry;
		unsigned int total_buckets;
	} levels[TDB_MAX_LEVELS + 1];
	unsigned int num_levels;
	unsigned int toplevel_group;
	/* This makes delete-everything-inside-traverse work as expected. */
	tdb_off_t prev;
};

enum tdb_lock_flags {
	/* WAIT == F_SETLKW, NOWAIT == F_SETLK */
	TDB_LOCK_NOWAIT = 0,
	TDB_LOCK_WAIT = 1,
	/* If set, don't log an error on failure. */
	TDB_LOCK_PROBE = 2,
	/* If set, don't check for recovery (used by recovery code). */
	TDB_LOCK_NOCHECK = 4,
};

struct tdb_lock {
	struct tdb_context *owner;
	off_t off;
	uint32_t count;
	uint32_t ltype;
};

/* This is only needed for tdb_access_commit, but used everywhere to
 * simplify. */
struct tdb_access_hdr {
	struct tdb_access_hdr *next;
	tdb_off_t off;
	tdb_len_t len;
	bool convert;
};

struct tdb_file {
	/* Single list of all TDBs, to detect multiple opens. */
	struct tdb_file *next;

	/* How many are sharing us? */
	unsigned int refcnt;

	/* Mmap (if any), or malloc (for TDB_INTERNAL). */
	void *map_ptr;

	/* How much space has been mapped (<= current file size) */
	tdb_len_t map_size;

	/* The file descriptor (-1 for TDB_INTERNAL). */
	int fd;

	/* Lock information */
	pid_t locker;
	struct tdb_lock allrecord_lock;
	size_t num_lockrecs;
	struct tdb_lock *lockrecs;

	/* Identity of this file. */
	dev_t device;
	ino_t inode;
};

struct tdb_context {
	/* Filename of the database. */
	const char *name;

	/* Are we accessing directly? (debugging check). */
	int direct_access;

	/* Operating read-only? (Opened O_RDONLY, or in traverse_read) */
	bool read_only;

	/* mmap read only? */
	int mmap_flags;

	/* the flags passed to tdb_open, for tdb_reopen. */
	uint32_t flags;

	/* Logging function */
	void (*log_fn)(struct tdb_context *tdb,
		       enum tdb_log_level level,
		       enum TDB_ERROR ecode,
		       const char *message,
		       void *data);
	void *log_data;

	/* Hash function. */
	uint64_t (*hash_fn)(const void *key, size_t len, uint64_t seed, void *);
	void *hash_data;
	uint64_t hash_seed;

	/* low level (fnctl) lock functions. */
	int (*lock_fn)(int fd, int rw, off_t off, off_t len, bool w, void *);
	int (*unlock_fn)(int fd, int rw, off_t off, off_t len, void *);
	void *lock_data;

	/* Set if we are in a transaction. */
	struct tdb_transaction *transaction;
	
	/* What free table are we using? */
	tdb_off_t ftable_off;
	unsigned int ftable;

	/* IO methods: changes for transactions. */
	const struct tdb_methods *methods;

	/* Our statistics. */
	struct tdb_attribute_stats stats;

	/* Direct access information */
	struct tdb_access_hdr *access;

	/* Last error we returned. */
	enum TDB_ERROR last_error;

	/* The actual file information */
	struct tdb_file *file;
};

struct tdb_methods {
	enum TDB_ERROR (*tread)(struct tdb_context *, tdb_off_t, void *,
				tdb_len_t);
	enum TDB_ERROR (*twrite)(struct tdb_context *, tdb_off_t, const void *,
				 tdb_len_t);
	enum TDB_ERROR (*oob)(struct tdb_context *, tdb_off_t, bool);
	enum TDB_ERROR (*expand_file)(struct tdb_context *, tdb_len_t);
	void *(*direct)(struct tdb_context *, tdb_off_t, size_t, bool);
};

/*
  internal prototypes
*/
/* hash.c: */
tdb_bool_err first_in_hash(struct tdb_context *tdb,
			   struct traverse_info *tinfo,
			   TDB_DATA *kbuf, size_t *dlen);

tdb_bool_err next_in_hash(struct tdb_context *tdb,
			  struct traverse_info *tinfo,
			  TDB_DATA *kbuf, size_t *dlen);

/* Hash random memory. */
uint64_t tdb_hash(struct tdb_context *tdb, const void *ptr, size_t len);

/* Hash on disk. */
uint64_t hash_record(struct tdb_context *tdb, tdb_off_t off);

/* Find and lock a hash entry (or where it would be). */
tdb_off_t find_and_lock(struct tdb_context *tdb,
			struct tdb_data key,
			int ltype,
			struct hash_info *h,
			struct tdb_used_record *rec,
			struct traverse_info *tinfo);

enum TDB_ERROR replace_in_hash(struct tdb_context *tdb,
			       struct hash_info *h,
			       tdb_off_t new_off);

enum TDB_ERROR add_to_hash(struct tdb_context *tdb, struct hash_info *h,
			   tdb_off_t new_off);

enum TDB_ERROR delete_from_hash(struct tdb_context *tdb, struct hash_info *h);

/* For tdb_check */
bool is_subhash(tdb_off_t val);

/* free.c: */
enum TDB_ERROR tdb_ftable_init(struct tdb_context *tdb);

/* check.c needs these to iterate through free lists. */
tdb_off_t first_ftable(struct tdb_context *tdb);
tdb_off_t next_ftable(struct tdb_context *tdb, tdb_off_t ftable);

/* This returns space or -ve error number. */
tdb_off_t alloc(struct tdb_context *tdb, size_t keylen, size_t datalen,
		uint64_t hash, unsigned magic, bool growing);

/* Put this record in a free list. */
enum TDB_ERROR add_free_record(struct tdb_context *tdb,
			       tdb_off_t off, tdb_len_t len_with_header,
			       enum tdb_lock_flags waitflag,
			       bool coalesce_ok);

/* Set up header for a used/ftable/htable/chain record. */
enum TDB_ERROR set_header(struct tdb_context *tdb,
			  struct tdb_used_record *rec,
			  unsigned magic, uint64_t keylen, uint64_t datalen,
			  uint64_t actuallen, unsigned hashlow);

/* Used by tdb_check to verify. */
unsigned int size_to_bucket(tdb_len_t data_len);
tdb_off_t bucket_off(tdb_off_t ftable_off, unsigned bucket);

/* Used by tdb_summary */
tdb_off_t dead_space(struct tdb_context *tdb, tdb_off_t off);

/* io.c: */
/* Initialize tdb->methods. */
void tdb_io_init(struct tdb_context *tdb);

/* Convert endian of the buffer if required. */
void *tdb_convert(const struct tdb_context *tdb, void *buf, tdb_len_t size);

/* Unmap and try to map the tdb. */
void tdb_munmap(struct tdb_file *file);
void tdb_mmap(struct tdb_context *tdb);

/* Either alloc a copy, or give direct access.  Release frees or noop. */
const void *tdb_access_read(struct tdb_context *tdb,
			    tdb_off_t off, tdb_len_t len, bool convert);
void *tdb_access_write(struct tdb_context *tdb,
		       tdb_off_t off, tdb_len_t len, bool convert);

/* Release result of tdb_access_read/write. */
void tdb_access_release(struct tdb_context *tdb, const void *p);
/* Commit result of tdb_acces_write. */
enum TDB_ERROR tdb_access_commit(struct tdb_context *tdb, void *p);

/* Convenience routine to get an offset. */
tdb_off_t tdb_read_off(struct tdb_context *tdb, tdb_off_t off);

/* Write an offset at an offset. */
enum TDB_ERROR tdb_write_off(struct tdb_context *tdb, tdb_off_t off,
			     tdb_off_t val);

/* Clear an ondisk area. */
enum TDB_ERROR zero_out(struct tdb_context *tdb, tdb_off_t off, tdb_len_t len);

/* Return a non-zero offset between >= start < end in this array (or end). */
tdb_off_t tdb_find_nonzero_off(struct tdb_context *tdb,
			       tdb_off_t base,
			       uint64_t start,
			       uint64_t end);

/* Return a zero offset in this array, or num. */
tdb_off_t tdb_find_zero_off(struct tdb_context *tdb, tdb_off_t off,
			    uint64_t num);

/* Allocate and make a copy of some offset. */
void *tdb_alloc_read(struct tdb_context *tdb, tdb_off_t offset, tdb_len_t len);

/* Writes a converted copy of a record. */
enum TDB_ERROR tdb_write_convert(struct tdb_context *tdb, tdb_off_t off,
				 const void *rec, size_t len);

/* Reads record and converts it */
enum TDB_ERROR tdb_read_convert(struct tdb_context *tdb, tdb_off_t off,
				void *rec, size_t len);

/* Bump the seqnum (caller checks for tdb->flags & TDB_SEQNUM) */
void tdb_inc_seqnum(struct tdb_context *tdb);

/* lock.c: */
/* Lock/unlock a range of hashes. */
enum TDB_ERROR tdb_lock_hashes(struct tdb_context *tdb,
			       tdb_off_t hash_lock, tdb_len_t hash_range,
			       int ltype, enum tdb_lock_flags waitflag);
enum TDB_ERROR tdb_unlock_hashes(struct tdb_context *tdb,
				 tdb_off_t hash_lock,
				 tdb_len_t hash_range, int ltype);

/* For closing the file. */
void tdb_lock_cleanup(struct tdb_context *tdb);

/* Lock/unlock a particular free bucket. */
enum TDB_ERROR tdb_lock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off,
				    enum tdb_lock_flags waitflag);
void tdb_unlock_free_bucket(struct tdb_context *tdb, tdb_off_t b_off);

/* Serialize transaction start. */
enum TDB_ERROR tdb_transaction_lock(struct tdb_context *tdb, int ltype);
void tdb_transaction_unlock(struct tdb_context *tdb, int ltype);

/* Do we have any hash locks (ie. via tdb_chainlock) ? */
bool tdb_has_hash_locks(struct tdb_context *tdb);

/* Lock entire database. */
enum TDB_ERROR tdb_allrecord_lock(struct tdb_context *tdb, int ltype,
				  enum tdb_lock_flags flags, bool upgradable);
void tdb_allrecord_unlock(struct tdb_context *tdb, int ltype);
enum TDB_ERROR tdb_allrecord_upgrade(struct tdb_context *tdb);

/* Serialize db open. */
enum TDB_ERROR tdb_lock_open(struct tdb_context *tdb,
			     int ltype, enum tdb_lock_flags flags);
void tdb_unlock_open(struct tdb_context *tdb, int ltype);
bool tdb_has_open_lock(struct tdb_context *tdb);

/* Serialize db expand. */
enum TDB_ERROR tdb_lock_expand(struct tdb_context *tdb, int ltype);
void tdb_unlock_expand(struct tdb_context *tdb, int ltype);
bool tdb_has_expansion_lock(struct tdb_context *tdb);

/* If it needs recovery, grab all the locks and do it. */
enum TDB_ERROR tdb_lock_and_recover(struct tdb_context *tdb);

/* Default lock and unlock functions. */
int tdb_fcntl_lock(int fd, int rw, off_t off, off_t len, bool waitflag, void *);
int tdb_fcntl_unlock(int fd, int rw, off_t off, off_t len, void *);

/* transaction.c: */
enum TDB_ERROR tdb_transaction_recover(struct tdb_context *tdb);
tdb_bool_err tdb_needs_recovery(struct tdb_context *tdb);

/* tdb.c: */
enum TDB_ERROR COLD tdb_logerr(struct tdb_context *tdb,
			       enum TDB_ERROR ecode,
			       enum tdb_log_level level,
			       const char *fmt, ...);

#ifdef TDB_TRACE
void tdb_trace(struct tdb_context *tdb, const char *op);
void tdb_trace_seqnum(struct tdb_context *tdb, uint32_t seqnum, const char *op);
void tdb_trace_open(struct tdb_context *tdb, const char *op,
		    unsigned hash_size, unsigned tdb_flags, unsigned open_flags);
void tdb_trace_ret(struct tdb_context *tdb, const char *op, int ret);
void tdb_trace_retrec(struct tdb_context *tdb, const char *op, TDB_DATA ret);
void tdb_trace_1rec(struct tdb_context *tdb, const char *op,
		    TDB_DATA rec);
void tdb_trace_1rec_ret(struct tdb_context *tdb, const char *op,
			TDB_DATA rec, int ret);
void tdb_trace_1rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec, TDB_DATA ret);
void tdb_trace_2rec_flag_ret(struct tdb_context *tdb, const char *op,
			     TDB_DATA rec1, TDB_DATA rec2, unsigned flag,
			     int ret);
void tdb_trace_2rec_retrec(struct tdb_context *tdb, const char *op,
			   TDB_DATA rec1, TDB_DATA rec2, TDB_DATA ret);
#else
#define tdb_trace(tdb, op)
#define tdb_trace_seqnum(tdb, seqnum, op)
#define tdb_trace_open(tdb, op, hash_size, tdb_flags, open_flags)
#define tdb_trace_ret(tdb, op, ret)
#define tdb_trace_retrec(tdb, op, ret)
#define tdb_trace_1rec(tdb, op, rec)
#define tdb_trace_1rec_ret(tdb, op, rec, ret)
#define tdb_trace_1rec_retrec(tdb, op, rec, ret)
#define tdb_trace_2rec_flag_ret(tdb, op, rec1, rec2, flag, ret)
#define tdb_trace_2rec_retrec(tdb, op, rec1, rec2, ret)
#endif /* !TDB_TRACE */

#endif
