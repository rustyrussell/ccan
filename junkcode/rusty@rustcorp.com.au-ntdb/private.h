#ifndef NTDB_PRIVATE_H
#define NTDB_PRIVATE_H
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

#include "config.h"
#ifndef HAVE_CCAN
#error You need ccan to build ntdb!
#endif
#include "ntdb.h"
#include <ccan/compiler/compiler.h>
#include <ccan/likely/likely.h>
#include <ccan/endian/endian.h>

#ifdef HAVE_LIBREPLACE
#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "system/shmem.h"
#include "system/select.h"
#include "system/wait.h"
#else
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
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
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#endif
#include <assert.h>

#ifndef TEST_IT
#define TEST_IT(cond)
#endif

/* #define NTDB_TRACE 1 */

#ifndef __STRING
#define __STRING(x)    #x
#endif

#ifndef __STRINGSTRING
#define __STRINGSTRING(x) __STRING(x)
#endif

#ifndef __location__
#define __location__ __FILE__ ":" __STRINGSTRING(__LINE__)
#endif

typedef uint64_t ntdb_len_t;
typedef uint64_t ntdb_off_t;

#define NTDB_MAGIC_FOOD "NTDB file\n"
#define NTDB_VERSION ((uint64_t)(0x26011967 + 7))
#define NTDB_USED_MAGIC ((uint64_t)0x1999)
#define NTDB_HTABLE_MAGIC ((uint64_t)0x1888)
#define NTDB_CHAIN_MAGIC ((uint64_t)0x1777)
#define NTDB_FTABLE_MAGIC ((uint64_t)0x1666)
#define NTDB_CAP_MAGIC ((uint64_t)0x1555)
#define NTDB_FREE_MAGIC ((uint64_t)0xFE)
#define NTDB_HASH_MAGIC (0xA1ABE11A01092008ULL)
#define NTDB_RECOVERY_MAGIC (0xf53bc0e7ad124589ULL)
#define NTDB_RECOVERY_INVALID_MAGIC (0x0ULL)

/* Capability bits. */
#define NTDB_CAP_TYPE_MASK	0x1FFFFFFFFFFFFFFFULL
#define NTDB_CAP_NOCHECK		0x8000000000000000ULL
#define NTDB_CAP_NOWRITE		0x4000000000000000ULL
#define NTDB_CAP_NOOPEN		0x2000000000000000ULL

#define NTDB_OFF_IS_ERR(off) unlikely(off >= (ntdb_off_t)(long)NTDB_ERR_LAST)
#define NTDB_OFF_TO_ERR(off) ((enum NTDB_ERROR)(long)(off))
#define NTDB_ERR_TO_OFF(ecode) ((ntdb_off_t)(long)(ecode))

/* Packing errors into pointers and v.v. */
#define NTDB_PTR_IS_ERR(ptr)						\
	unlikely((unsigned long)(ptr) >= (unsigned long)NTDB_ERR_LAST)
#define NTDB_PTR_ERR(p) ((enum NTDB_ERROR)(long)(p))
#define NTDB_ERR_PTR(err) ((void *)(long)(err))

/* This doesn't really need to be pagesize, but we use it for similar
 * reasons. */
#define NTDB_PGSIZE 16384

/* Common case of returning true, false or -ve error. */
typedef int ntdb_bool_err;

/* Prevent others from opening the file. */
#define NTDB_OPEN_LOCK 0
/* Expanding file. */
#define NTDB_EXPANSION_LOCK 2
/* Doing a transaction. */
#define NTDB_TRANSACTION_LOCK 8
/* Hash chain locks. */
#define NTDB_HASH_LOCK_START 64

/* Extend file by least 100 times larger than needed. */
#define NTDB_EXTENSION_FACTOR 100

/* We steal this many upper bits, giving a maximum offset of 64 exabytes. */
#define NTDB_OFF_UPPER_STEAL 8

/* And we use the lower bit, too. */
#define NTDB_OFF_CHAIN_BIT	0

/* Hash table sits just after the header. */
#define NTDB_HASH_OFFSET (sizeof(struct ntdb_header))

/* Additional features we understand.  Currently: none. */
#define NTDB_FEATURE_MASK ((uint64_t)0)

/* The bit number where we store the extra hash bits. */
/* Convenience mask to get actual offset. */
#define NTDB_OFF_MASK							\
	(((1ULL << (64 - NTDB_OFF_UPPER_STEAL)) - 1) - (1<<NTDB_OFF_CHAIN_BIT))

/* How many buckets in a free list: see size_to_bucket(). */
#define NTDB_FREE_BUCKETS (64 - NTDB_OFF_UPPER_STEAL)

/* We have to be able to fit a free record here. */
#define NTDB_MIN_DATA_LEN						\
	(sizeof(struct ntdb_free_record) - sizeof(struct ntdb_used_record))

/* Indicates this entry is not on an flist (can happen during coalescing) */
#define NTDB_FTABLE_NONE ((1ULL << NTDB_OFF_UPPER_STEAL) - 1)

/* By default, hash is 64k bytes */
#define NTDB_DEFAULT_HBITS 13

struct ntdb_used_record {
	/* For on-disk compatibility, we avoid bitfields:
	   magic: 16,        (highest)
	   key_len_bits: 5,
	   extra_padding: 32
	*/
        uint64_t magic_and_meta;
	/* The bottom key_len_bits*2 are key length, rest is data length. */
        uint64_t key_and_data_len;
};

static inline unsigned rec_key_bits(const struct ntdb_used_record *r)
{
	return ((r->magic_and_meta >> 43) & ((1 << 5)-1)) * 2;
}

static inline uint64_t rec_key_length(const struct ntdb_used_record *r)
{
	return r->key_and_data_len & ((1ULL << rec_key_bits(r)) - 1);
}

static inline uint64_t rec_data_length(const struct ntdb_used_record *r)
{
	return r->key_and_data_len >> rec_key_bits(r);
}

static inline uint64_t rec_extra_padding(const struct ntdb_used_record *r)
{
	return (r->magic_and_meta >> 11) & 0xFFFFFFFF;
}

static inline uint16_t rec_magic(const struct ntdb_used_record *r)
{
	return (r->magic_and_meta >> 48);
}

struct ntdb_free_record {
        uint64_t magic_and_prev; /* NTDB_OFF_UPPER_STEAL bits magic, then prev */
        uint64_t ftable_and_len; /* Len not counting these two fields. */
	/* This is why the minimum record size is 8 bytes.  */
	uint64_t next;
};

static inline uint64_t frec_prev(const struct ntdb_free_record *f)
{
	return f->magic_and_prev & ((1ULL << (64 - NTDB_OFF_UPPER_STEAL)) - 1);
}

static inline uint64_t frec_magic(const struct ntdb_free_record *f)
{
	return f->magic_and_prev >> (64 - NTDB_OFF_UPPER_STEAL);
}

static inline uint64_t frec_len(const struct ntdb_free_record *f)
{
	return f->ftable_and_len & ((1ULL << (64 - NTDB_OFF_UPPER_STEAL))-1);
}

static inline unsigned frec_ftable(const struct ntdb_free_record *f)
{
	return f->ftable_and_len >> (64 - NTDB_OFF_UPPER_STEAL);
}

struct ntdb_recovery_record {
	uint64_t magic;
	/* Length of record (add this header to get total length). */
	uint64_t max_len;
	/* Length used. */
	uint64_t len;
	/* Old length of file before transaction. */
	uint64_t eof;
};

/* this is stored at the front of every database */
struct ntdb_header {
	char magic_food[64]; /* for /etc/magic */
	/* FIXME: Make me 32 bit? */
	uint64_t version; /* version of the code */
	uint64_t hash_bits; /* bits for toplevel hash table. */
	uint64_t hash_test; /* result of hashing HASH_MAGIC. */
	uint64_t hash_seed; /* "random" seed written at creation time. */
	ntdb_off_t free_table; /* (First) free table. */
	ntdb_off_t recovery; /* Transaction recovery area. */

	uint64_t features_used; /* Features all writers understand */
	uint64_t features_offered; /* Features offered */

	uint64_t seqnum; /* Sequence number for NTDB_SEQNUM */

	ntdb_off_t capabilities; /* Optional linked list of capabilities. */
	ntdb_off_t reserved[22];

	/*
	 * Hash table is next:
	 *
	 * struct ntdb_used_record htable_hdr;
	 * ntdb_off_t htable[1 << hash_bits];
	 */
};

struct ntdb_freetable {
	struct ntdb_used_record hdr;
	ntdb_off_t next;
	ntdb_off_t buckets[NTDB_FREE_BUCKETS];
};

struct ntdb_capability {
	struct ntdb_used_record hdr;
	ntdb_off_t type;
	ntdb_off_t next;
	/* ... */
};

/* Information about a particular (locked) hash entry. */
struct hash_info {
	/* Full hash value of entry. */
	uint32_t h;
	/* Start of hash table / chain. */
	ntdb_off_t table;
	/* Number of entries in this table/chain. */
	ntdb_off_t table_size;
	/* Bucket we (or an empty space) were found in. */
	ntdb_off_t bucket;
	/* Old value that was in that entry (if not found) */
	ntdb_off_t old_val;
};

enum ntdb_lock_flags {
	/* WAIT == F_SETLKW, NOWAIT == F_SETLK */
	NTDB_LOCK_NOWAIT = 0,
	NTDB_LOCK_WAIT = 1,
	/* If set, don't log an error on failure. */
	NTDB_LOCK_PROBE = 2,
	/* If set, don't check for recovery (used by recovery code). */
	NTDB_LOCK_NOCHECK = 4,
};

struct ntdb_lock {
	struct ntdb_context *owner;
	off_t off;
	uint32_t count;
	uint32_t ltype;
};

/* This is only needed for ntdb_access_commit, but used everywhere to
 * simplify. */
struct ntdb_access_hdr {
	struct ntdb_access_hdr *next;
	ntdb_off_t off;
	ntdb_len_t len;
	bool convert;
};

/* mmaps we are keeping around because they are still direct accessed */
struct ntdb_old_mmap {
	struct ntdb_old_mmap *next;

	void *map_ptr;
	ntdb_len_t map_size;
};

struct ntdb_file {
	/* How many are sharing us? */
	unsigned int refcnt;

	/* Mmap (if any), or malloc (for NTDB_INTERNAL). */
	void *map_ptr;

	/* How much space has been mapped (<= current file size) */
	ntdb_len_t map_size;

	/* The file descriptor (-1 for NTDB_INTERNAL). */
	int fd;

	/* How many are accessing directly? */
	unsigned int direct_count;

	/* Old maps, still direct accessed. */
	struct ntdb_old_mmap *old_mmaps;

	/* Lock information */
	pid_t locker;
	struct ntdb_lock allrecord_lock;
	size_t num_lockrecs;
	struct ntdb_lock *lockrecs;

	/* Identity of this file. */
	dev_t device;
	ino_t inode;
};

struct ntdb_methods {
	enum NTDB_ERROR (*tread)(struct ntdb_context *, ntdb_off_t, void *,
				 ntdb_len_t);
	enum NTDB_ERROR (*twrite)(struct ntdb_context *, ntdb_off_t, const void *,
				  ntdb_len_t);
	enum NTDB_ERROR (*oob)(struct ntdb_context *, ntdb_off_t, ntdb_len_t, bool);
	enum NTDB_ERROR (*expand_file)(struct ntdb_context *, ntdb_len_t);
	void *(*direct)(struct ntdb_context *, ntdb_off_t, size_t, bool);
	ntdb_off_t (*read_off)(struct ntdb_context *ntdb, ntdb_off_t off);
	enum NTDB_ERROR (*write_off)(struct ntdb_context *ntdb, ntdb_off_t off,
				     ntdb_off_t val);
};

/*
  internal prototypes
*/
/* Get bits from a value. */
static inline uint32_t bits_from(uint64_t val, unsigned start, unsigned num)
{
	assert(num <= 32);
	return (val >> start) & ((1U << num) - 1);
}


/* hash.c: */
uint32_t ntdb_jenkins_hash(const void *key, size_t length, uint32_t seed,
			   void *unused);

enum NTDB_ERROR first_in_hash(struct ntdb_context *ntdb,
			      struct hash_info *h,
			      NTDB_DATA *kbuf, size_t *dlen);

enum NTDB_ERROR next_in_hash(struct ntdb_context *ntdb,
			     struct hash_info *h,
			     NTDB_DATA *kbuf, size_t *dlen);

/* Hash random memory. */
uint32_t ntdb_hash(struct ntdb_context *ntdb, const void *ptr, size_t len);

/* Find and lock a hash entry (or where it would be). */
ntdb_off_t find_and_lock(struct ntdb_context *ntdb,
			 NTDB_DATA key,
			 int ltype,
			 struct hash_info *h,
			 struct ntdb_used_record *rec,
			 const char **rkey);

enum NTDB_ERROR replace_in_hash(struct ntdb_context *ntdb,
				const struct hash_info *h,
				ntdb_off_t new_off);

enum NTDB_ERROR add_to_hash(struct ntdb_context *ntdb,
			    const struct hash_info *h,
			    ntdb_off_t new_off);

enum NTDB_ERROR delete_from_hash(struct ntdb_context *ntdb,
				 const struct hash_info *h);

/* For ntdb_check */
bool is_subhash(ntdb_off_t val);
enum NTDB_ERROR unknown_capability(struct ntdb_context *ntdb, const char *caller,
				   ntdb_off_t type);

/* free.c: */
enum NTDB_ERROR ntdb_ftable_init(struct ntdb_context *ntdb);

/* check.c needs these to iterate through free lists. */
ntdb_off_t first_ftable(struct ntdb_context *ntdb);
ntdb_off_t next_ftable(struct ntdb_context *ntdb, ntdb_off_t ftable);

/* This returns space or -ve error number. */
ntdb_off_t alloc(struct ntdb_context *ntdb, size_t keylen, size_t datalen,
		 unsigned magic, bool growing);

/* Put this record in a free list. */
enum NTDB_ERROR add_free_record(struct ntdb_context *ntdb,
				ntdb_off_t off, ntdb_len_t len_with_header,
				enum ntdb_lock_flags waitflag,
				bool coalesce_ok);

/* Set up header for a used/ftable/htable/chain/capability record. */
enum NTDB_ERROR set_header(struct ntdb_context *ntdb,
			   struct ntdb_used_record *rec,
			   unsigned magic, uint64_t keylen, uint64_t datalen,
			   uint64_t actuallen);

/* Used by ntdb_check to verify. */
unsigned int size_to_bucket(ntdb_len_t data_len);
ntdb_off_t bucket_off(ntdb_off_t ftable_off, unsigned bucket);

/* Used by ntdb_summary */
ntdb_off_t dead_space(struct ntdb_context *ntdb, ntdb_off_t off);

/* Adjust expansion, used by create_recovery_area */
ntdb_off_t ntdb_expand_adjust(ntdb_off_t map_size, ntdb_off_t size);

/* io.c: */
/* Initialize ntdb->methods. */
void ntdb_io_init(struct ntdb_context *ntdb);

/* Convert endian of the buffer if required. */
void *ntdb_convert(const struct ntdb_context *ntdb, void *buf, ntdb_len_t size);

/* Unmap and try to map the ntdb. */
enum NTDB_ERROR ntdb_munmap(struct ntdb_context *ntdb);
enum NTDB_ERROR ntdb_mmap(struct ntdb_context *ntdb);

/* Either alloc a copy, or give direct access.  Release frees or noop. */
const void *ntdb_access_read(struct ntdb_context *ntdb,
			     ntdb_off_t off, ntdb_len_t len, bool convert);
void *ntdb_access_write(struct ntdb_context *ntdb,
			ntdb_off_t off, ntdb_len_t len, bool convert);

/* Release result of ntdb_access_read/write. */
void ntdb_access_release(struct ntdb_context *ntdb, const void *p);
/* Commit result of ntdb_acces_write. */
enum NTDB_ERROR ntdb_access_commit(struct ntdb_context *ntdb, void *p);

/* Clear an ondisk area. */
enum NTDB_ERROR zero_out(struct ntdb_context *ntdb, ntdb_off_t off, ntdb_len_t len);

/* Return a non-zero offset between >= start < end in this array (or end). */
ntdb_off_t ntdb_find_nonzero_off(struct ntdb_context *ntdb,
				 ntdb_off_t base,
				 uint64_t start,
				 uint64_t end);

/* Return a zero offset in this array, or num. */
ntdb_off_t ntdb_find_zero_off(struct ntdb_context *ntdb, ntdb_off_t off,
			      uint64_t num);

/* Allocate and make a copy of some offset. */
void *ntdb_alloc_read(struct ntdb_context *ntdb, ntdb_off_t offset, ntdb_len_t len);

/* Writes a converted copy of a record. */
enum NTDB_ERROR ntdb_write_convert(struct ntdb_context *ntdb, ntdb_off_t off,
				   const void *rec, size_t len);

/* Reads record and converts it */
enum NTDB_ERROR ntdb_read_convert(struct ntdb_context *ntdb, ntdb_off_t off,
				  void *rec, size_t len);

/* Bump the seqnum (caller checks for ntdb->flags & NTDB_SEQNUM) */
void ntdb_inc_seqnum(struct ntdb_context *ntdb);

/* lock.c: */
/* Print message because another ntdb owns a lock we want. */
enum NTDB_ERROR owner_conflict(struct ntdb_context *ntdb, const char *call);

/* If we fork, we no longer really own locks. */
bool check_lock_pid(struct ntdb_context *ntdb, const char *call, bool log);

/* Lock/unlock a hash bucket. */
enum NTDB_ERROR ntdb_lock_hash(struct ntdb_context *ntdb,
			       unsigned int hbucket,
			       int ltype);
enum NTDB_ERROR ntdb_unlock_hash(struct ntdb_context *ntdb,
				 unsigned int hash, int ltype);

/* For closing the file. */
void ntdb_lock_cleanup(struct ntdb_context *ntdb);

/* Lock/unlock a particular free bucket. */
enum NTDB_ERROR ntdb_lock_free_bucket(struct ntdb_context *ntdb, ntdb_off_t b_off,
				      enum ntdb_lock_flags waitflag);
void ntdb_unlock_free_bucket(struct ntdb_context *ntdb, ntdb_off_t b_off);

/* Serialize transaction start. */
enum NTDB_ERROR ntdb_transaction_lock(struct ntdb_context *ntdb, int ltype);
void ntdb_transaction_unlock(struct ntdb_context *ntdb, int ltype);

/* Do we have any hash locks (ie. via ntdb_chainlock) ? */
bool ntdb_has_hash_locks(struct ntdb_context *ntdb);

/* Lock entire database. */
enum NTDB_ERROR ntdb_allrecord_lock(struct ntdb_context *ntdb, int ltype,
				    enum ntdb_lock_flags flags, bool upgradable);
void ntdb_allrecord_unlock(struct ntdb_context *ntdb, int ltype);
enum NTDB_ERROR ntdb_allrecord_upgrade(struct ntdb_context *ntdb, off_t start);

/* Serialize db open. */
enum NTDB_ERROR ntdb_lock_open(struct ntdb_context *ntdb,
			       int ltype, enum ntdb_lock_flags flags);
void ntdb_unlock_open(struct ntdb_context *ntdb, int ltype);
bool ntdb_has_open_lock(struct ntdb_context *ntdb);

/* Serialize db expand. */
enum NTDB_ERROR ntdb_lock_expand(struct ntdb_context *ntdb, int ltype);
void ntdb_unlock_expand(struct ntdb_context *ntdb, int ltype);
bool ntdb_has_expansion_lock(struct ntdb_context *ntdb);

/* If it needs recovery, grab all the locks and do it. */
enum NTDB_ERROR ntdb_lock_and_recover(struct ntdb_context *ntdb);

/* Default lock and unlock functions. */
int ntdb_fcntl_lock(int fd, int rw, off_t off, off_t len, bool waitflag, void *);
int ntdb_fcntl_unlock(int fd, int rw, off_t off, off_t len, void *);

/* transaction.c: */
enum NTDB_ERROR ntdb_transaction_recover(struct ntdb_context *ntdb);
ntdb_bool_err ntdb_needs_recovery(struct ntdb_context *ntdb);

struct ntdb_context {
	/* Single list of all TDBs, to detect multiple opens. */
	struct ntdb_context *next;

	/* Filename of the database. */
	const char *name;

	/* Logging function */
	void (*log_fn)(struct ntdb_context *ntdb,
		       enum ntdb_log_level level,
		       enum NTDB_ERROR ecode,
		       const char *message,
		       void *data);
	void *log_data;

	/* Open flags passed to ntdb_open. */
	int open_flags;

	/* low level (fnctl) lock functions. */
	int (*lock_fn)(int fd, int rw, off_t off, off_t len, bool w, void *);
	int (*unlock_fn)(int fd, int rw, off_t off, off_t len, void *);
	void *lock_data;

	/* the ntdb flags passed to ntdb_open. */
	uint32_t flags;

	/* Our statistics. */
	struct ntdb_attribute_stats stats;

	/* The actual file information */
	struct ntdb_file *file;

	/* Hash function. */
	uint32_t (*hash_fn)(const void *key, size_t len, uint32_t seed, void *);
	void *hash_data;
	uint32_t hash_seed;
	/* Bits in toplevel hash table. */
	unsigned int hash_bits;

	/* Allocate and free functions. */
	void *(*alloc_fn)(const void *owner, size_t len, void *priv_data);
	void *(*expand_fn)(void *old, size_t newlen, void *priv_data);
	void (*free_fn)(void *old, void *priv_data);
	void *alloc_data;

	/* Our open hook, if any. */
	enum NTDB_ERROR (*openhook)(int fd, void *data);
	void *openhook_data;

	/* Set if we are in a transaction. */
	struct ntdb_transaction *transaction;

	/* What free table are we using? */
	ntdb_off_t ftable_off;
	unsigned int ftable;

	/* IO methods: changes for transactions. */
	const struct ntdb_methods *io;

	/* Direct access information */
	struct ntdb_access_hdr *access;
};

/* ntdb.c: */
enum NTDB_ERROR COLD PRINTF_FMT(4, 5)
	ntdb_logerr(struct ntdb_context *ntdb,
		    enum NTDB_ERROR ecode,
		    enum ntdb_log_level level,
		    const char *fmt, ...);

static inline enum NTDB_ERROR ntdb_oob(struct ntdb_context *ntdb,
				       ntdb_off_t off, ntdb_len_t len,
				       bool probe)
{
	if (likely(off + len >= off)
	    && likely(off + len <= ntdb->file->map_size)
	    && likely(!probe)) {
		    return NTDB_SUCCESS;
	}
	return ntdb->io->oob(ntdb, off, len, probe);
}

/* Convenience routine to get an offset. */
static inline ntdb_off_t ntdb_read_off(struct ntdb_context *ntdb,
				       ntdb_off_t off)
{
	return ntdb->io->read_off(ntdb, off);
}

/* Write an offset at an offset. */
static inline enum NTDB_ERROR ntdb_write_off(struct ntdb_context *ntdb,
					     ntdb_off_t off,
			       ntdb_off_t val)
{
	return ntdb->io->write_off(ntdb, off, val);
}

#ifdef NTDB_TRACE
void ntdb_trace(struct ntdb_context *ntdb, const char *op);
void ntdb_trace_seqnum(struct ntdb_context *ntdb, uint32_t seqnum, const char *op);
void ntdb_trace_open(struct ntdb_context *ntdb, const char *op,
		     unsigned hash_size, unsigned ntdb_flags, unsigned open_flags);
void ntdb_trace_ret(struct ntdb_context *ntdb, const char *op, int ret);
void ntdb_trace_retrec(struct ntdb_context *ntdb, const char *op, NTDB_DATA ret);
void ntdb_trace_1rec(struct ntdb_context *ntdb, const char *op,
		     NTDB_DATA rec);
void ntdb_trace_1rec_ret(struct ntdb_context *ntdb, const char *op,
			 NTDB_DATA rec, int ret);
void ntdb_trace_1rec_retrec(struct ntdb_context *ntdb, const char *op,
			    NTDB_DATA rec, NTDB_DATA ret);
void ntdb_trace_2rec_flag_ret(struct ntdb_context *ntdb, const char *op,
			      NTDB_DATA rec1, NTDB_DATA rec2, unsigned flag,
			      int ret);
void ntdb_trace_2rec_retrec(struct ntdb_context *ntdb, const char *op,
			    NTDB_DATA rec1, NTDB_DATA rec2, NTDB_DATA ret);
#else
#define ntdb_trace(ntdb, op)
#define ntdb_trace_seqnum(ntdb, seqnum, op)
#define ntdb_trace_open(ntdb, op, hash_size, ntdb_flags, open_flags)
#define ntdb_trace_ret(ntdb, op, ret)
#define ntdb_trace_retrec(ntdb, op, ret)
#define ntdb_trace_1rec(ntdb, op, rec)
#define ntdb_trace_1rec_ret(ntdb, op, rec, ret)
#define ntdb_trace_1rec_retrec(ntdb, op, rec, ret)
#define ntdb_trace_2rec_flag_ret(ntdb, op, rec1, rec2, flag, ret)
#define ntdb_trace_2rec_retrec(ntdb, op, rec1, rec2, ret)
#endif /* !NTDB_TRACE */

#endif
