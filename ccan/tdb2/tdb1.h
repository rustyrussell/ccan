#ifndef TDB1_H
#define TDB1_H

/*
   Unix SMB/CIFS implementation.

   trivial database library (version 1 compat functions)

   Copyright (C) Andrew Tridgell 1999-2004
   Copyright (C) Rusty Russell 2011

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
#include "tdb2.h"

#ifndef _SAMBA_BUILD_
/* For mode_t */
#include <sys/types.h>
/* For O_* flags. */
#include <sys/stat.h>
#endif

/** Flags to tdb1_store() */
#define TDB1_REPLACE 1		/** Unused */
#define TDB1_INSERT 2 		/** Don't overwrite an existing entry */
#define TDB1_MODIFY 3		/** Don't create an existing entry    */

/** Flags for tdb1_open() */
#define TDB1_DEFAULT 0 /** just a readability place holder */
#define TDB1_CLEAR_IF_FIRST 1 /** If this is the first open, wipe the db */
#define TDB1_INTERNAL 2 /** Don't store on disk */
#define TDB1_NOLOCK   4 /** Don't do any locking */
#define TDB1_NOMMAP   8 /** Don't use mmap */
#define TDB1_CONVERT 16 /** Convert endian (internal use) */
#define TDB1_BIGENDIAN 32 /** Header is big-endian (internal use) */
#define TDB1_NOSYNC   64 /** Don't use synchronous transactions */
#define TDB1_SEQNUM   128 /** Maintain a sequence number */
#define TDB1_VOLATILE   256 /** Activate the per-hashchain freelist, default 5 */
#define TDB1_ALLOW_NESTING 512 /** Allow transactions to nest */
#define TDB1_DISALLOW_NESTING 1024 /** Disallow transactions to nest */
#define TDB1_INCOMPATIBLE_HASH 2048 /** Better hashing: can't be opened by tdb < 1.2.6. */

/** The tdb error codes */
enum TDB1_ERROR {TDB1_SUCCESS=0, TDB1_ERR_CORRUPT, TDB1_ERR_IO, TDB1_ERR_LOCK,
		TDB1_ERR_OOM, TDB1_ERR_EXISTS, TDB1_ERR_NOLOCK, TDB1_ERR_LOCK_TIMEOUT,
		TDB1_ERR_NOEXIST, TDB1_ERR_EINVAL, TDB1_ERR_RDONLY,
		TDB1_ERR_NESTING};

/** Debugging uses one of the following levels */
enum tdb1_debug_level {TDB1_DEBUG_FATAL = 0, TDB1_DEBUG_ERROR,
		      TDB1_DEBUG_WARNING, TDB1_DEBUG_TRACE};

/** The tdb data structure */
typedef struct TDB1_DATA {
	unsigned char *dptr;
	size_t dsize;
} TDB1_DATA;

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3)
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

/** This is the context structure that is returned from a db open. */
typedef struct tdb1_context TDB1_CONTEXT;

typedef int (*tdb1_traverse_func)(struct tdb1_context *, TDB1_DATA, TDB1_DATA, void *);
typedef void (*tdb1_log_func)(struct tdb1_context *, enum tdb1_debug_level, const char *, ...) PRINTF_ATTRIBUTE(3, 4);
typedef unsigned int (*tdb1_hash_func)(TDB1_DATA *key);

struct tdb1_logging_context {
        tdb1_log_func log_fn;
        void *log_private;
};

struct tdb1_context *tdb1_open(const char *name, int hash_size, int tdb1_flags,
		      int open_flags, mode_t mode);

struct tdb1_context *tdb1_open_ex(const char *name, int hash_size, int tdb1_flags,
			 int open_flags, mode_t mode,
			 const struct tdb1_logging_context *log_ctx,
			 tdb1_hash_func hash_fn);

void tdb1_set_max_dead(struct tdb1_context *tdb, int max_dead);

TDB1_DATA tdb1_fetch(struct tdb1_context *tdb, TDB1_DATA key);

int tdb1_parse_record(struct tdb1_context *tdb, TDB1_DATA key,
			      int (*parser)(TDB1_DATA key, TDB1_DATA data,
					    void *private_data),
			      void *private_data);

int tdb1_delete(struct tdb1_context *tdb, TDB1_DATA key);

int tdb1_store(struct tdb1_context *tdb, TDB1_DATA key, TDB1_DATA dbuf, int flag);

int tdb1_append(struct tdb1_context *tdb, TDB1_DATA key, TDB1_DATA new_dbuf);

int tdb1_close(struct tdb1_context *tdb);

TDB1_DATA tdb1_firstkey(struct tdb1_context *tdb);

TDB1_DATA tdb1_nextkey(struct tdb1_context *tdb, TDB1_DATA key);

int tdb1_traverse(struct tdb1_context *tdb, tdb1_traverse_func fn, void *private_data);

int tdb1_traverse_read(struct tdb1_context *tdb, tdb1_traverse_func fn, void *private_data);

int tdb1_exists(struct tdb1_context *tdb, TDB1_DATA key);

int tdb1_lockall(struct tdb1_context *tdb);

int tdb1_unlockall(struct tdb1_context *tdb);

int tdb1_lockall_read(struct tdb1_context *tdb);

int tdb1_unlockall_read(struct tdb1_context *tdb);

tdb1_log_func tdb1_log_fn(struct tdb1_context *tdb);

int tdb1_transaction_start(struct tdb1_context *tdb);

int tdb1_transaction_prepare_commit(struct tdb1_context *tdb);

int tdb1_transaction_commit(struct tdb1_context *tdb);

int tdb1_transaction_cancel(struct tdb1_context *tdb);

int tdb1_get_seqnum(struct tdb1_context *tdb);

int tdb1_hash_size(struct tdb1_context *tdb);

void tdb1_increment_seqnum_nonblock(struct tdb1_context *tdb);

unsigned int tdb1_jenkins_hash(TDB1_DATA *key);

int tdb1_check(struct tdb1_context *tdb,
	      int (*check) (TDB1_DATA key, TDB1_DATA data, void *private_data),
	      void *private_data);

/* @} ******************************************************************/

/* Low level locking functions: use with care */
int tdb1_chainlock(struct tdb1_context *tdb, TDB1_DATA key);
int tdb1_chainunlock(struct tdb1_context *tdb, TDB1_DATA key);
int tdb1_chainlock_read(struct tdb1_context *tdb, TDB1_DATA key);
int tdb1_chainunlock_read(struct tdb1_context *tdb, TDB1_DATA key);


/* wipe and repack */
int tdb1_wipe_all(struct tdb1_context *tdb);
int tdb1_repack(struct tdb1_context *tdb);

/* Debug functions. Not used in production. */
char *tdb1_summary(struct tdb1_context *tdb);

extern TDB1_DATA tdb1_null;

#endif /* tdb1.h */
