#ifndef CCAN_TDB2_TDB1_PRIVATE_H
#define CCAN_TDB2_TDB1_PRIVATE_H
 /*
   Unix SMB/CIFS implementation.

   trivial database library - private includes

   Copyright (C) Andrew Tridgell              2005

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

#ifndef _SAMBA_BUILD_
#include "config.h"
/* This keeps us consistent with TDB2 code. */
#if HAVE_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <utime.h>

#ifndef _PUBLIC_
#define _PUBLIC_
#endif

#else
#include "replace.h"
#include "system/filesys.h"
#include "system/time.h"
#include "system/shmem.h"
#include "system/select.h"
#include "system/wait.h"
#endif

#include "tdb1.h"

/* #define TDB_TRACE 1 */
#ifndef HAVE_GETPAGESIZE
#define getpagesize() 0x2000
#endif

#ifndef __STRING
#define __STRING(x)    #x
#endif

#ifndef __STRINGSTRING
#define __STRINGSTRING(x) __STRING(x)
#endif

#ifndef __location__
#define __location__ __FILE__ ":" __STRINGSTRING(__LINE__)
#endif

typedef uint32_t tdb1_len_t;
typedef uint32_t tdb1_off_t;

#ifndef offsetof
#define offsetof(t,f) ((unsigned int)&((t *)0)->f)
#endif

#define TDB1_MAGIC_FOOD "TDB file\n"
#define TDB1_VERSION (0x26011967 + 6)
#define TDB1_MAGIC (0x26011999U)
#define TDB1_FREE_MAGIC (~TDB1_MAGIC)
#define TDB1_DEAD_MAGIC (0xFEE1DEAD)
#define TDB1_RECOVERY_MAGIC (0xf53bc0e7U)
#define TDB1_RECOVERY_INVALID_MAGIC (0x0)
#define TDB1_HASH_RWLOCK_MAGIC (0xbad1a51U)
#define TDB1_ALIGNMENT 4
#define TDB1_DEFAULT_HASH_SIZE 131
#define TDB1_FREELIST_TOP (sizeof(struct tdb1_header))
#define TDB1_ALIGN(x,a) (((x) + (a)-1) & ~((a)-1))
#define TDB1_BYTEREV(x) (((((x)&0xff)<<24)|((x)&0xFF00)<<8)|(((x)>>8)&0xFF00)|((x)>>24))
#define TDB1_DEAD(r) ((r)->magic == TDB1_DEAD_MAGIC)
#define TDB1_BAD_MAGIC(r) ((r)->magic != TDB1_MAGIC && !TDB1_DEAD(r))
#define TDB1_HASH_TOP(hash) (TDB1_FREELIST_TOP + (TDB1_BUCKET(hash)+1)*sizeof(tdb1_off_t))
#define TDB1_HASHTABLE_SIZE(tdb) ((tdb->header.hash_size+1)*sizeof(tdb1_off_t))
#define TDB1_DATA_START(hash_size) (TDB1_HASH_TOP(hash_size-1) + sizeof(tdb1_off_t))
#define TDB1_RECOVERY_HEAD offsetof(struct tdb1_header, recovery_start)
#define TDB1_SEQNUM_OFS    offsetof(struct tdb1_header, sequence_number)
#define TDB1_PAD_BYTE 0x42
#define TDB1_PAD_U32  0x42424242

/* NB assumes there is a local variable called "tdb" that is the
 * current context, also takes doubly-parenthesized print-style
 * argument. */
#define TDB1_LOG(x) tdb->log.log_fn x

/* lock offsets */
#define TDB1_OPEN_LOCK        0
#define TDB1_ACTIVE_LOCK      4
#define TDB1_TRANSACTION_LOCK 8

/* free memory if the pointer is valid and zero the pointer */
#ifndef SAFE_FREE
#define SAFE_FREE(x) do { if ((x) != NULL) {free(x); (x)=NULL;} } while(0)
#endif

#define TDB1_BUCKET(hash) ((hash) % tdb->header.hash_size)

#define TDB1_DOCONV() (tdb->flags & TDB1_CONVERT)
#define TDB1_CONV(x) (TDB1_DOCONV() ? tdb1_convert(&x, sizeof(x)) : &x)

/* the body of the database is made of one tdb1_record for the free space
   plus a separate data list for each hash value */
struct tdb1_record {
	tdb1_off_t next; /* offset of the next record in the list */
	tdb1_len_t rec_len; /* total byte length of record */
	tdb1_len_t key_len; /* byte length of key */
	tdb1_len_t data_len; /* byte length of data */
	uint32_t full_hash; /* the full 32 bit hash of the key */
	uint32_t magic;   /* try to catch errors */
	/* the following union is implied:
		union {
			char record[rec_len];
			struct {
				char key[key_len];
				char data[data_len];
			}
			uint32_t totalsize; (tailer)
		}
	*/
};


/* this is stored at the front of every database */
struct tdb1_header {
	char magic_food[32]; /* for /etc/magic */
	uint32_t version; /* version of the code */
	uint32_t hash_size; /* number of hash entries */
	tdb1_off_t rwlocks; /* obsolete - kept to detect old formats */
	tdb1_off_t recovery_start; /* offset of transaction recovery region */
	tdb1_off_t sequence_number; /* used when TDB1_SEQNUM is set */
	uint32_t magic1_hash; /* hash of TDB1_MAGIC_FOOD. */
	uint32_t magic2_hash; /* hash of TDB1_MAGIC. */
	tdb1_off_t reserved[27];
};

struct tdb1_lock_type {
	uint32_t off;
	uint32_t count;
	uint32_t ltype;
};

struct tdb1_traverse_lock {
	struct tdb1_traverse_lock *next;
	uint32_t off;
	uint32_t hash;
	int lock_rw;
};

enum tdb1_lock_flags {
	/* WAIT == F_SETLKW, NOWAIT == F_SETLK */
	TDB1_LOCK_NOWAIT = 0,
	TDB1_LOCK_WAIT = 1,
	/* If set, don't log an error on failure. */
	TDB1_LOCK_PROBE = 2,
	/* If set, don't actually lock at all. */
	TDB1_LOCK_MARK_ONLY = 4,
};

struct tdb1_context;

struct tdb1_methods {
	int (*tdb1_read)(struct tdb1_context *, tdb1_off_t , void *, tdb1_len_t , int );
	int (*tdb1_write)(struct tdb1_context *, tdb1_off_t, const void *, tdb1_len_t);
	void (*next_hash_chain)(struct tdb1_context *, uint32_t *);
	int (*tdb1_oob)(struct tdb1_context *, tdb1_off_t , int );
	int (*tdb1_expand_file)(struct tdb1_context *, tdb1_off_t , tdb1_off_t );
};

struct tdb1_context {
	char *name; /* the name of the database */
	void *map_ptr; /* where it is currently mapped */
	int fd; /* open file descriptor for the database */
	tdb1_len_t map_size; /* how much space has been mapped */
	int read_only; /* opened read-only */
	int traverse_read; /* read-only traversal */
	int traverse_write; /* read-write traversal */
	struct tdb1_lock_type allrecord_lock; /* .offset == upgradable */
	int num_lockrecs;
	struct tdb1_lock_type *lockrecs; /* only real locks, all with count>0 */
	enum TDB1_ERROR ecode; /* error code for last tdb error */
	struct tdb1_header header; /* a cached copy of the header */
	uint32_t flags; /* the flags passed to tdb1_open */
	struct tdb1_traverse_lock travlocks; /* current traversal locks */
	struct tdb1_context *next; /* all tdbs to avoid multiple opens */
	dev_t device;	/* uniquely identifies this tdb */
	ino_t inode;	/* uniquely identifies this tdb */
	struct tdb1_logging_context log;
	unsigned int (*hash_fn)(TDB1_DATA *key);
	int open_flags; /* flags used in the open - needed by reopen */
	const struct tdb1_methods *methods;
	struct tdb1_transaction *transaction;
	int page_size;
	int max_dead_records;
#ifdef TDB1_TRACE
	int tracefd;
#endif
	volatile sig_atomic_t *interrupt_sig_ptr;
};


/*
  internal prototypes
*/
int tdb1_munmap(struct tdb1_context *tdb);
void tdb1_mmap(struct tdb1_context *tdb);
int tdb1_lock(struct tdb1_context *tdb, int list, int ltype);
int tdb1_lock_nonblock(struct tdb1_context *tdb, int list, int ltype);
int tdb1_nest_lock(struct tdb1_context *tdb, uint32_t offset, int ltype,
		  enum tdb1_lock_flags flags);
int tdb1_nest_unlock(struct tdb1_context *tdb, uint32_t offset, int ltype,
		    bool mark_lock);
int tdb1_unlock(struct tdb1_context *tdb, int list, int ltype);
int tdb1_brlock(struct tdb1_context *tdb,
	       int rw_type, tdb1_off_t offset, size_t len,
	       enum tdb1_lock_flags flags);
int tdb1_brunlock(struct tdb1_context *tdb,
		 int rw_type, tdb1_off_t offset, size_t len);
bool tdb1_have_extra_locks(struct tdb1_context *tdb);
void tdb1_release_transaction_locks(struct tdb1_context *tdb);
int tdb1_transaction_lock(struct tdb1_context *tdb, int ltype,
			 enum tdb1_lock_flags lockflags);
int tdb1_transaction_unlock(struct tdb1_context *tdb, int ltype);
int tdb1_recovery_area(struct tdb1_context *tdb,
		      const struct tdb1_methods *methods,
		      tdb1_off_t *recovery_offset,
		      struct tdb1_record *rec);
int tdb1_allrecord_lock(struct tdb1_context *tdb, int ltype,
		       enum tdb1_lock_flags flags, bool upgradable);
int tdb1_allrecord_unlock(struct tdb1_context *tdb, int ltype, bool mark_lock);
int tdb1_allrecord_upgrade(struct tdb1_context *tdb);
int tdb1_write_lock_record(struct tdb1_context *tdb, tdb1_off_t off);
int tdb1_write_unlock_record(struct tdb1_context *tdb, tdb1_off_t off);
int tdb1_ofs_read(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_off_t *d);
int tdb1_ofs_write(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_off_t *d);
void *tdb1_convert(void *buf, uint32_t size);
int tdb1_free(struct tdb1_context *tdb, tdb1_off_t offset, struct tdb1_record *rec);
tdb1_off_t tdb1_allocate(struct tdb1_context *tdb, tdb1_len_t length, struct tdb1_record *rec);
int tdb1_ofs_read(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_off_t *d);
int tdb1_ofs_write(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_off_t *d);
int tdb1_lock_record(struct tdb1_context *tdb, tdb1_off_t off);
int tdb1_unlock_record(struct tdb1_context *tdb, tdb1_off_t off);
bool tdb1_needs_recovery(struct tdb1_context *tdb);
int tdb1_rec_read(struct tdb1_context *tdb, tdb1_off_t offset, struct tdb1_record *rec);
int tdb1_rec_write(struct tdb1_context *tdb, tdb1_off_t offset, struct tdb1_record *rec);
int tdb1_do_delete(struct tdb1_context *tdb, tdb1_off_t rec_ptr, struct tdb1_record *rec);
unsigned char *tdb1_alloc_read(struct tdb1_context *tdb, tdb1_off_t offset, tdb1_len_t len);
int tdb1_parse_data(struct tdb1_context *tdb, TDB1_DATA key,
		   tdb1_off_t offset, tdb1_len_t len,
		   int (*parser)(TDB1_DATA key, TDB1_DATA data,
				 void *private_data),
		   void *private_data);
tdb1_off_t tdb1_find_lock_hash(struct tdb1_context *tdb, TDB1_DATA key, uint32_t hash, int locktype,
			   struct tdb1_record *rec);
void tdb1_io_init(struct tdb1_context *tdb);
int tdb1_expand(struct tdb1_context *tdb, tdb1_off_t size);
int tdb1_rec_free_read(struct tdb1_context *tdb, tdb1_off_t off,
		      struct tdb1_record *rec);
bool tdb1_write_all(int fd, const void *buf, size_t count);
int tdb1_transaction_recover(struct tdb1_context *tdb);
void tdb1_header_hash(struct tdb1_context *tdb,
		     uint32_t *magic1_hash, uint32_t *magic2_hash);
unsigned int tdb1_old_hash(TDB1_DATA *key);
size_t tdb1_dead_space(struct tdb1_context *tdb, tdb1_off_t off);
#endif /* CCAN_TDB2_TDB1_PRIVATE_H */
