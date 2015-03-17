#ifndef CCAN_NTDB_H
#define CCAN_NTDB_H

/*
   NTDB: trivial database library version 2

   Copyright (C) Andrew Tridgell 1999-2004
   Copyright (C) Rusty Russell 2010-2012

     ** NOTE! The following LGPL license applies to the ntdb
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

#ifdef  __cplusplus
extern "C" {
#endif

#ifdef HAVE_LIBREPLACE
#include <replace.h>
#include <system/filesys.h>
#else
#if HAVE_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#ifndef _PUBLIC_
#ifdef HAVE_VISIBILITY_ATTR
#define _PUBLIC_ __attribute__((visibility("default")))
#else
#define _PUBLIC_
#endif
#endif

/* For mode_t */
#include <sys/types.h>
/* For O_* flags. */
#include <sys/stat.h>
/* For sig_atomic_t. */
#include <signal.h>
/* For uint64_t */
#include <stdint.h>
/* For bool */
#include <stdbool.h>
/* For memcmp */
#include <string.h>
#endif

#if HAVE_CCAN
#include <ccan/compiler/compiler.h>
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/cast/cast.h>
#else
#ifndef typesafe_cb_preargs
/* Failing to have CCAN just mean less typesafe protection, etc. */
#define typesafe_cb_preargs(rtype, atype, fn, arg, ...)	\
	((rtype (*)(__VA_ARGS__, atype))(fn))
#endif
#ifndef cast_const
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
#define cast_const(type, expr) ((type)((intptr_t)(expr)))
#else
#define cast_const(type, expr) ((type *)(expr))
#endif
#endif
#endif /* !HAVE_CCAN */

union ntdb_attribute;
struct ntdb_context;

/**
 * struct TDB_DATA - (n)tdb data blob
 *
 * To ease compatibility, we use 'struct TDB_DATA' from tdb.h, so if
 * you want to include both tdb.h and ntdb.h, you need to #include
 * tdb.h first.
 */
#ifndef __TDB_H__
struct TDB_DATA {
	unsigned char *dptr;
	size_t dsize;
};
#endif

typedef struct TDB_DATA NTDB_DATA;

/**
 * ntdb_open - open a database file
 * @name: the file name (or database name if flags contains NTDB_INTERNAL)
 * @ntdb_flags: options for this database
 * @open_flags: flags argument for ntdb's open() call.
 * @mode: mode argument for ntdb's open() call.
 * @attributes: linked list of extra attributes for this ntdb.
 *
 * This call opens (and potentially creates) a database file.
 * Multiple processes can have the NTDB file open at once.
 *
 * On failure it will return NULL, and set errno: it may also call
 * any log attribute found in @attributes.
 *
 * See also:
 *	union ntdb_attribute
 */
struct ntdb_context *ntdb_open(const char *name, int ntdb_flags,
			       int open_flags, mode_t mode,
			       union ntdb_attribute *attributes);


/* flags for ntdb_open() */
#define NTDB_DEFAULT 0 /* just a readability place holder */
#define NTDB_INTERNAL 2 /* don't store on disk */
#define NTDB_NOLOCK   4 /* don't do any locking */
#define NTDB_NOMMAP   8 /* don't use mmap */
#define NTDB_CONVERT 16 /* convert endian */
#define NTDB_NOSYNC   64 /* don't use synchronous transactions */
#define NTDB_SEQNUM   128 /* maintain a sequence number */
#define NTDB_ALLOW_NESTING   256 /* fake nested transactions */
#define NTDB_RDONLY   512 /* implied by O_RDONLY */
#define NTDB_CANT_CHECK  2048 /* has a feature which we don't understand */

/**
 * ntdb_close - close and free a ntdb.
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This always succeeds, in that @ntdb is unusable after this call.  But if
 * some unexpected error occurred while closing, it will return non-zero
 * (the only clue as to cause will be via the log attribute).
 */
int ntdb_close(struct ntdb_context *ntdb);

/**
 * enum NTDB_ERROR - error returns for NTDB
 *
 * See Also:
 *	ntdb_errorstr()
 */
enum NTDB_ERROR {
	NTDB_SUCCESS	= 0,	/* No error. */
	NTDB_ERR_CORRUPT = -1,	/* We read the db, and it was bogus. */
	NTDB_ERR_IO	= -2,	/* We couldn't read/write the db. */
	NTDB_ERR_LOCK	= -3,	/* Locking failed. */
	NTDB_ERR_OOM	= -4,	/* Out of Memory. */
	NTDB_ERR_EXISTS	= -5,	/* The key already exists. */
	NTDB_ERR_NOEXIST	= -6,	/* The key does not exist. */
	NTDB_ERR_EINVAL	= -7,	/* You're using it wrong. */
	NTDB_ERR_RDONLY	= -8,	/* The database is read-only. */
	NTDB_ERR_LAST = NTDB_ERR_RDONLY
};

/**
 * ntdb_store - store a key/value pair in a ntdb.
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key
 * @dbuf: the data to associate with the key.
 * @flag: NTDB_REPLACE, NTDB_INSERT or NTDB_MODIFY.
 *
 * This inserts (or overwrites) a key/value pair in the NTDB.  If flag
 * is NTDB_REPLACE, it doesn't matter whether the key exists or not;
 * NTDB_INSERT means it must not exist (returns NTDB_ERR_EXISTS otherwise),
 * and NTDB_MODIFY means it must exist (returns NTDB_ERR_NOEXIST otherwise).
 *
 * On success, this returns NTDB_SUCCESS.
 *
 * See also:
 *	ntdb_fetch, ntdb_transaction_start, ntdb_append, ntdb_delete.
 */
enum NTDB_ERROR ntdb_store(struct ntdb_context *ntdb,
			   NTDB_DATA key,
			   NTDB_DATA dbuf,
			   int flag);

/* flags to ntdb_store() */
#define NTDB_REPLACE 1		/* A readability place holder */
#define NTDB_INSERT 2 		/* Don't overwrite an existing entry */
#define NTDB_MODIFY 3		/* Don't create an existing entry    */

/**
 * ntdb_fetch - fetch a value from a ntdb.
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key
 * @data: pointer to data.
 *
 * This looks up a key in the database and sets it in @data.
 *
 * If it returns NTDB_SUCCESS, the key was found: it is your
 * responsibility to call free() on @data->dptr.
 *
 * Otherwise, it returns an error (usually, NTDB_ERR_NOEXIST) and @data is
 * undefined.
 */
enum NTDB_ERROR ntdb_fetch(struct ntdb_context *ntdb, NTDB_DATA key,
			   NTDB_DATA *data);

/**
 * ntdb_errorstr - map the ntdb error onto a constant readable string
 * @ecode: the enum NTDB_ERROR to map.
 *
 * This is useful for displaying errors to users.
 */
const char *ntdb_errorstr(enum NTDB_ERROR ecode);

/**
 * ntdb_append - append a value to a key/value pair in a ntdb.
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key
 * @dbuf: the data to append.
 *
 * This is equivalent to fetching a record, reallocating .dptr to add the
 * data, and writing it back, only it's much more efficient.  If the key
 * doesn't exist, it's equivalent to ntdb_store (with an additional hint that
 * you expect to expand the record in future).
 *
 * See Also:
 *	ntdb_fetch(), ntdb_store()
 */
enum NTDB_ERROR ntdb_append(struct ntdb_context *ntdb,
			    NTDB_DATA key, NTDB_DATA dbuf);

/**
 * ntdb_delete - delete a key from a ntdb.
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to delete.
 *
 * Returns NTDB_SUCCESS on success, or an error (usually NTDB_ERR_NOEXIST).
 *
 * See Also:
 *	ntdb_fetch(), ntdb_store()
 */
enum NTDB_ERROR ntdb_delete(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_exists - does a key exist in the database?
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to search for.
 *
 * Returns true if it exists, or false if it doesn't or any other error.
 */
bool ntdb_exists(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_deq - are NTDB_DATA equal?
 * @a: one NTDB_DATA
 * @b: another NTDB_DATA
 */
static inline bool ntdb_deq(NTDB_DATA a, NTDB_DATA b)
{
	return a.dsize == b.dsize && memcmp(a.dptr, b.dptr, a.dsize) == 0;
}

/**
 * ntdb_mkdata - make a NTDB_DATA from const data
 * @p: the constant pointer
 * @len: the length
 *
 * As the dptr member of NTDB_DATA is not constant, you need to
 * cast it.  This function keeps thost casts in one place, as well as
 * suppressing the warning some compilers give when casting away a
 * qualifier (eg. gcc with -Wcast-qual)
 */
static inline NTDB_DATA ntdb_mkdata(const void *p, size_t len)
{
	NTDB_DATA d;
	d.dptr = cast_const(void *, p);
	d.dsize = len;
	return d;
}

/**
 * ntdb_transaction_start - start a transaction
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This begins a series of atomic operations.  Other processes will be able
 * to read the ntdb, but not alter it (they will block), nor will they see
 * any changes until ntdb_transaction_commit() is called.
 *
 * Note that if the NTDB_ALLOW_NESTING flag is set, a ntdb_transaction_start()
 * within a transaction will succeed, but it's not a real transaction:
 * (1) An inner transaction which is committed is not actually committed until
 *     the outer transaction is; if the outer transaction is cancelled, the
 *     inner ones are discarded.
 * (2) ntdb_transaction_cancel() marks the outer transaction as having an error,
 *     so the final ntdb_transaction_commit() will fail.
 * (3) the outer transaction will see the results of the inner transaction.
 *
 * See Also:
 *	ntdb_transaction_cancel, ntdb_transaction_commit.
 */
enum NTDB_ERROR ntdb_transaction_start(struct ntdb_context *ntdb);

/**
 * ntdb_transaction_cancel - abandon a transaction
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This aborts a transaction, discarding any changes which were made.
 * ntdb_close() does this implicitly.
 */
void ntdb_transaction_cancel(struct ntdb_context *ntdb);

/**
 * ntdb_transaction_commit - commit a transaction
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This completes a transaction, writing any changes which were made.
 *
 * fsync() is used to commit the transaction (unless NTDB_NOSYNC is set),
 * making it robust against machine crashes, but very slow compared to
 * other NTDB operations.
 *
 * A failure can only be caused by unexpected errors (eg. I/O or
 * memory); this is no point looping on transaction failure.
 *
 * See Also:
 *	ntdb_transaction_prepare_commit()
 */
enum NTDB_ERROR ntdb_transaction_commit(struct ntdb_context *ntdb);

/**
 * ntdb_transaction_prepare_commit - prepare to commit a transaction
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This ensures we have the resources to commit a transaction (using
 * ntdb_transaction_commit): if this succeeds then a transaction will only
 * fail if the write() or fsync() calls fail.
 *
 * If this fails you must still call ntdb_transaction_cancel() to cancel
 * the transaction.
 *
 * See Also:
 *	ntdb_transaction_commit()
 */
enum NTDB_ERROR ntdb_transaction_prepare_commit(struct ntdb_context *ntdb);

/**
 * ntdb_traverse - traverse a NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 * @fn: the function to call for every key/value pair (or NULL)
 * @p: the pointer to hand to @f
 *
 * This walks the NTDB until all they keys have been traversed, or @fn
 * returns non-zero.  If the traverse function or other processes are
 * changing data or adding or deleting keys, the traverse may be
 * unreliable: keys may be skipped or (rarely) visited twice.
 *
 * There is one specific exception: the special case of deleting the
 * current key does not undermine the reliability of the traversal.
 *
 * On success, returns the number of keys iterated.  On error returns
 * a negative enum NTDB_ERROR value.
 */
#define ntdb_traverse(ntdb, fn, p)					\
	ntdb_traverse_(ntdb, typesafe_cb_preargs(int, void *, (fn), (p), \
						 struct ntdb_context *,	\
						 NTDB_DATA, NTDB_DATA), (p))

int64_t ntdb_traverse_(struct ntdb_context *ntdb,
		       int (*fn)(struct ntdb_context *,
				 NTDB_DATA, NTDB_DATA, void *), void *p);

/**
 * ntdb_parse_record - operate directly on data in the database.
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key whose record we should hand to @parse
 * @parse: the function to call for the data
 * @data: the private pointer to hand to @parse (types must match).
 *
 * This avoids a copy for many cases, by handing you a pointer into
 * the memory-mapped database.  It also locks the record to prevent
 * other accesses at the same time, so it won't change.
 *
 * Within the @parse callback you can perform read operations on the
 * database, but no write operations: no ntdb_store() or
 * ntdb_delete(), for example.  The exception is if you call
 * ntdb_lockall() before ntdb_parse_record().
 *
 * Never alter the data handed to parse()!
 */
#define ntdb_parse_record(ntdb, key, parse, data)			\
	ntdb_parse_record_((ntdb), (key),				\
			   typesafe_cb_preargs(enum NTDB_ERROR, void *,	\
					       (parse), (data),		\
					       NTDB_DATA, NTDB_DATA), (data))

enum NTDB_ERROR ntdb_parse_record_(struct ntdb_context *ntdb,
				   NTDB_DATA key,
				   enum NTDB_ERROR (*parse)(NTDB_DATA k,
							    NTDB_DATA d,
							    void *data),
				   void *data);

/**
 * ntdb_get_seqnum - get a database sequence number
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This returns a sequence number: any change to the database from a
 * ntdb context opened with the NTDB_SEQNUM flag will cause that number
 * to increment.  Note that the incrementing is unreliable (it is done
 * without locking), so this is only useful as an optimization.
 *
 * For example, you may have a regular database backup routine which
 * does not operate if the sequence number is unchanged.  In the
 * unlikely event of a failed increment, it will be backed up next
 * time any way.
 *
 * Returns an enum NTDB_ERROR (ie. negative) on error.
 */
int64_t ntdb_get_seqnum(struct ntdb_context *ntdb);

/**
 * ntdb_firstkey - get the "first" key in a NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: pointer to key.
 *
 * This returns an arbitrary key in the database; with ntdb_nextkey() it allows
 * open-coded traversal of the database, though it is slightly less efficient
 * than ntdb_traverse.
 *
 * It is your responsibility to free @key->dptr on success.
 *
 * Returns NTDB_ERR_NOEXIST if the database is empty.
 */
enum NTDB_ERROR ntdb_firstkey(struct ntdb_context *ntdb, NTDB_DATA *key);

/**
 * ntdb_nextkey - get the "next" key in a NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: a key returned by ntdb_firstkey() or ntdb_nextkey().
 *
 * This returns another key in the database; it will free @key.dptr for
 * your convenience.
 *
 * Returns NTDB_ERR_NOEXIST if there are no more keys.
 */
enum NTDB_ERROR ntdb_nextkey(struct ntdb_context *ntdb, NTDB_DATA *key);

/**
 * ntdb_chainlock - lock a record in the NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to lock.
 *
 * This prevents any access occurring to a group of keys including @key,
 * even if @key does not exist.  This allows primitive atomic updates of
 * records without using transactions.
 *
 * You cannot begin a transaction while holding a ntdb_chainlock(), nor can
 * you do any operations on any other keys in the database.  This also means
 * that you cannot hold more than one ntdb_chainlock() at a time.
 *
 * See Also:
 *	ntdb_chainunlock()
 */
enum NTDB_ERROR ntdb_chainlock(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_chainunlock - unlock a record in the NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to unlock.
 *
 * The key must have previously been locked by ntdb_chainlock().
 */
void ntdb_chainunlock(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_chainlock_read - lock a record in the NTDB, for reading
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to lock.
 *
 * This prevents any changes from occurring to a group of keys including @key,
 * even if @key does not exist.  This allows primitive atomic updates of
 * records without using transactions.
 *
 * You cannot begin a transaction while holding a ntdb_chainlock_read(), nor can
 * you do any operations on any other keys in the database.  This also means
 * that you cannot hold more than one ntdb_chainlock()/read() at a time.
 *
 * See Also:
 *	ntdb_chainlock()
 */
enum NTDB_ERROR ntdb_chainlock_read(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_chainunlock_read - unlock a record in the NTDB for reading
 * @ntdb: the ntdb context returned from ntdb_open()
 * @key: the key to unlock.
 *
 * The key must have previously been locked by ntdb_chainlock_read().
 */
void ntdb_chainunlock_read(struct ntdb_context *ntdb, NTDB_DATA key);

/**
 * ntdb_lockall - lock the entire NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * You cannot hold a ntdb_chainlock while calling this.  It nests, so you
 * must call ntdb_unlockall as many times as you call ntdb_lockall.
 */
enum NTDB_ERROR ntdb_lockall(struct ntdb_context *ntdb);

/**
 * ntdb_unlockall - unlock the entire NTDB
 * @ntdb: the ntdb context returned from ntdb_open()
 */
void ntdb_unlockall(struct ntdb_context *ntdb);

/**
 * ntdb_lockall_read - lock the entire NTDB for reading
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This prevents others writing to the database, eg. ntdb_delete, ntdb_store,
 * ntdb_append, but not ntdb_fetch.
 *
 * You cannot hold a ntdb_chainlock while calling this.  It nests, so you
 * must call ntdb_unlockall_read as many times as you call ntdb_lockall_read.
 */
enum NTDB_ERROR ntdb_lockall_read(struct ntdb_context *ntdb);

/**
 * ntdb_unlockall_read - unlock the entire NTDB for reading
 * @ntdb: the ntdb context returned from ntdb_open()
 */
void ntdb_unlockall_read(struct ntdb_context *ntdb);

/**
 * ntdb_wipe_all - wipe the database clean
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * Completely erase the database.  This is faster than iterating through
 * each key and doing ntdb_delete.
 */
enum NTDB_ERROR ntdb_wipe_all(struct ntdb_context *ntdb);

/**
 * ntdb_repack - repack the database
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This repacks the database; if it is suffering from a great deal of
 * fragmentation this might help.  However, it can take twice the
 * memory of the existing NTDB.
 */
enum NTDB_ERROR ntdb_repack(struct ntdb_context *ntdb);

/**
 * ntdb_check - check a NTDB for consistency
 * @ntdb: the ntdb context returned from ntdb_open()
 * @check: function to check each key/data pair (or NULL)
 * @data: argument for @check, must match type.
 *
 * This performs a consistency check of the open database, optionally calling
 * a check() function on each record so you can do your own data consistency
 * checks as well.  If check() returns an error, that is returned from
 * ntdb_check().
 *
 * Note that the NTDB uses a feature which we don't understand which
 * indicates we can't run ntdb_check(), this will log a warning to that
 * effect and return NTDB_SUCCESS.  You can detect this condition by
 * looking for NTDB_CANT_CHECK in ntdb_get_flags().
 *
 * Returns NTDB_SUCCESS or an error.
 */
#define ntdb_check(ntdb, check, data)					\
	ntdb_check_((ntdb), typesafe_cb_preargs(enum NTDB_ERROR, void *, \
						(check), (data),	\
						NTDB_DATA,		\
						NTDB_DATA),		\
		    (data))

enum NTDB_ERROR ntdb_check_(struct ntdb_context *ntdb,
			    enum NTDB_ERROR (*check)(NTDB_DATA k,
						     NTDB_DATA d,
						     void *data),
			    void *data);

/**
 * enum ntdb_summary_flags - flags for ntdb_summary.
 */
enum ntdb_summary_flags {
	NTDB_SUMMARY_HISTOGRAMS = 1 /* Draw graphs in the summary. */
};

/**
 * ntdb_summary - return a string describing the NTDB state
 * @ntdb: the ntdb context returned from ntdb_open()
 * @flags: flags to control the summary output.
 * @summary: pointer to string to allocate.
 *
 * This returns a developer-readable string describing the overall
 * state of the ntdb, such as the percentage used and sizes of records.
 * It is designed to provide information about the ntdb at a glance
 * without displaying any keys or data in the database.
 *
 * On success, sets @summary to point to a malloc()'ed nul-terminated
 * multi-line string.  It is your responsibility to free() it.
 */
enum NTDB_ERROR ntdb_summary(struct ntdb_context *ntdb,
			     enum ntdb_summary_flags flags,
			     char **summary);


/**
 * ntdb_get_flags - return the flags for a ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This returns the flags on the current ntdb.  Some of these are caused by
 * the flags argument to ntdb_open(), others (such as NTDB_CONVERT) are
 * intuited.
 */
unsigned int ntdb_get_flags(struct ntdb_context *ntdb);

/**
 * ntdb_add_flag - set a flag for a ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 * @flag: one of NTDB_NOLOCK, NTDB_NOMMAP, NTDB_NOSYNC or NTDB_ALLOW_NESTING.
 *
 * You can use this to set a flag on the NTDB.  You cannot set these flags
 * on a NTDB_INTERNAL ntdb.
 */
void ntdb_add_flag(struct ntdb_context *ntdb, unsigned flag);

/**
 * ntdb_remove_flag - unset a flag for a ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 * @flag: one of NTDB_NOLOCK, NTDB_NOMMAP, NTDB_NOSYNC or NTDB_ALLOW_NESTING.
 *
 * You can use this to clear a flag on the NTDB.  You cannot clear flags
 * on a NTDB_INTERNAL ntdb.
 */
void ntdb_remove_flag(struct ntdb_context *ntdb, unsigned flag);

/**
 * enum ntdb_attribute_type - descriminator for union ntdb_attribute.
 */
enum ntdb_attribute_type {
	NTDB_ATTRIBUTE_LOG = 0,
	NTDB_ATTRIBUTE_HASH = 1,
	NTDB_ATTRIBUTE_SEED = 2,
	NTDB_ATTRIBUTE_STATS = 3,
	NTDB_ATTRIBUTE_OPENHOOK = 4,
	NTDB_ATTRIBUTE_FLOCK = 5,
	NTDB_ATTRIBUTE_ALLOCATOR = 6,
	NTDB_ATTRIBUTE_HASHSIZE = 7
};

/**
 * ntdb_get_attribute - get an attribute for an existing ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 * @attr: the union ntdb_attribute to set.
 *
 * This gets an attribute from a NTDB which has previously been set (or
 * may return the default values).  Set @attr.base.attr to the
 * attribute type you want get.
 */
enum NTDB_ERROR ntdb_get_attribute(struct ntdb_context *ntdb,
				   union ntdb_attribute *attr);

/**
 * ntdb_set_attribute - set an attribute for an existing ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 * @attr: the union ntdb_attribute to set.
 *
 * This sets an attribute on a NTDB, overriding any previous attribute
 * of the same type.  It returns NTDB_ERR_EINVAL if the attribute is
 * unknown or invalid.
 *
 * Note that NTDB_ATTRIBUTE_HASH, NTDB_ATTRIBUTE_SEED, and
 * NTDB_ATTRIBUTE_OPENHOOK cannot currently be set after ntdb_open.
 */
enum NTDB_ERROR ntdb_set_attribute(struct ntdb_context *ntdb,
				   const union ntdb_attribute *attr);

/**
 * ntdb_unset_attribute - reset an attribute for an existing ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 * @type: the attribute type to unset.
 *
 * This unsets an attribute on a NTDB, returning it to the defaults
 * (where applicable).
 *
 * Note that it only makes sense for NTDB_ATTRIBUTE_LOG and NTDB_ATTRIBUTE_FLOCK
 * to be unset.
 */
void ntdb_unset_attribute(struct ntdb_context *ntdb,
			  enum ntdb_attribute_type type);

/**
 * ntdb_name - get the name of a ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This returns a copy of the name string, made at ntdb_open() time.
 *
 * This is mostly useful for logging.
 */
const char *ntdb_name(const struct ntdb_context *ntdb);

/**
 * ntdb_fd - get the file descriptor of a ntdb
 * @ntdb: the ntdb context returned from ntdb_open()
 *
 * This returns the file descriptor for the underlying database file, or -1
 * for NTDB_INTERNAL.
 */
int ntdb_fd(const struct ntdb_context *ntdb);

/**
 * ntdb_foreach - iterate through every open NTDB.
 * @fn: the function to call for every NTDB
 * @p: the pointer to hand to @fn
 *
 * NTDB internally keeps track of all open TDBs; this function allows you to
 * iterate through them.  If @fn returns non-zero, traversal stops.
 */
#define ntdb_foreach(fn, p)						\
	ntdb_foreach_(typesafe_cb_preargs(int, void *, (fn), (p),	\
					  struct ntdb_context *), (p))

void ntdb_foreach_(int (*fn)(struct ntdb_context *, void *), void *p);

/**
 * struct ntdb_attribute_base - common fields for all ntdb attributes.
 */
struct ntdb_attribute_base {
	enum ntdb_attribute_type attr;
	union ntdb_attribute *next;
};

/**
 * enum ntdb_log_level - log levels for ntdb_attribute_log
 * @NTDB_LOG_ERROR: used to log unrecoverable errors such as I/O errors
 *		   or internal consistency failures.
 * @NTDB_LOG_USE_ERROR: used to log usage errors such as invalid parameters
 *		   or writing to a read-only database.
 * @NTDB_LOG_WARNING: used for informational messages on issues which
 *		     are unusual but handled by NTDB internally, such
 *		     as a failure to mmap or failure to open /dev/urandom.
 *		     It's also used when ntdb_open() fails without O_CREAT
 *		     because a file does not exist.
 */
enum ntdb_log_level {
	NTDB_LOG_ERROR,
	NTDB_LOG_USE_ERROR,
	NTDB_LOG_WARNING
};

/**
 * struct ntdb_attribute_log - log function attribute
 *
 * This attribute provides a hook for you to log errors.
 */
struct ntdb_attribute_log {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_LOG */
	void (*fn)(struct ntdb_context *ntdb,
		   enum ntdb_log_level level,
		   enum NTDB_ERROR ecode,
		   const char *message,
		   void *data);
	void *data;
};

/**
 * struct ntdb_attribute_hash - hash function attribute
 *
 * This attribute allows you to provide an alternative hash function.
 * This hash function will be handed keys from the database; it will also
 * be handed the 8-byte NTDB_HASH_MAGIC value for checking the header (the
 * ntdb_open() will fail if the hash value doesn't match the header).
 *
 * Note that if your hash function gives different results on
 * different machine endians, your ntdb will no longer work across
 * different architectures!
 */
struct ntdb_attribute_hash {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_HASH */
	uint32_t (*fn)(const void *key, size_t len, uint32_t seed,
		       void *data);
	void *data;
};

/**
 * struct ntdb_attribute_seed - hash function seed attribute
 *
 * The hash function seed is normally taken from /dev/urandom (or equivalent)
 * but can be set manually here.  This is mainly for testing purposes.
 */
struct ntdb_attribute_seed {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_SEED */
	uint64_t seed;
};

/**
 * struct ntdb_attribute_stats - ntdb operational statistics
 *
 * This attribute records statistics of various low-level NTDB operations.
 * This can be used to assist performance evaluation.  This is only
 * useful for ntdb_get_attribute().
 *
 * New fields will be added at the end, hence the "size" argument which
 * indicates how large your structure is: it must be filled in before
 * calling ntdb_get_attribute(), which will overwrite it with the size
 * ntdb knows about.
 */
struct ntdb_attribute_stats {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_STATS */
	size_t size; /* = sizeof(struct ntdb_attribute_stats) */
	uint64_t allocs;
	uint64_t   alloc_subhash;
	uint64_t   alloc_chain;
	uint64_t   alloc_bucket_exact;
	uint64_t   alloc_bucket_max;
	uint64_t   alloc_leftover;
	uint64_t   alloc_coalesce_tried;
	uint64_t     alloc_coalesce_iterate_clash;
	uint64_t     alloc_coalesce_lockfail;
	uint64_t     alloc_coalesce_race;
	uint64_t     alloc_coalesce_succeeded;
	uint64_t       alloc_coalesce_num_merged;
	uint64_t compares;
	uint64_t   compare_wrong_offsetbits;
	uint64_t   compare_wrong_keylen;
	uint64_t   compare_wrong_rechash;
	uint64_t   compare_wrong_keycmp;
	uint64_t transactions;
	uint64_t   transaction_cancel;
	uint64_t   transaction_nest;
	uint64_t   transaction_expand_file;
	uint64_t   transaction_read_direct;
	uint64_t      transaction_read_direct_fail;
	uint64_t   transaction_write_direct;
	uint64_t      transaction_write_direct_fail;
	uint64_t traverses;
	uint64_t	traverse_val_vanished;
	uint64_t expands;
	uint64_t frees;
	uint64_t locks;
	uint64_t   lock_lowlevel;
	uint64_t   lock_nonblock;
	uint64_t     lock_nonblock_fail;
};

/**
 * struct ntdb_attribute_openhook - ntdb special effects hook for open
 *
 * This attribute contains a function to call once we have the OPEN_LOCK
 * for the ntdb, but before we've examined its contents.  If this succeeds,
 * the ntdb will be populated if it's then zero-length.
 *
 * This is a hack to allow support for TDB-style TDB_CLEAR_IF_FIRST
 * behaviour.
 */
struct ntdb_attribute_openhook {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_OPENHOOK */
	enum NTDB_ERROR (*fn)(int fd, void *data);
	void *data;
};

/**
 * struct ntdb_attribute_flock - ntdb special effects hook for file locking
 *
 * This attribute contains function to call to place locks on a file; it can
 * be used to support non-blocking operations or lock proxying.
 *
 * They should return 0 on success, -1 on failure and set errno.
 *
 * An error will be logged on error if errno is neither EAGAIN nor EINTR
 * (normally it would only return EAGAIN if waitflag is false, and
 * loop internally on EINTR).
 */
struct ntdb_attribute_flock {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_FLOCK */
	int (*lock)(int fd,int rw, off_t off, off_t len, bool waitflag, void *);
	int (*unlock)(int fd, int rw, off_t off, off_t len, void *);
	void *data;
};

/**
 * struct ntdb_attribute_hashsize - ntdb hashsize setting.
 *
 * This attribute is only settable on ntdb_open; it indicates that we create
 * a hashtable of the given size, rather than the default.
 */
struct ntdb_attribute_hashsize {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_HASHSIZE */
	uint32_t size;
};

/**
 * struct ntdb_attribute_allocator - allocator for ntdb to use.
 *
 * You can replace malloc/free with your own allocation functions.
 * The allocator takes an "owner" pointer, which is either NULL (for
 * the initial struct ntdb_context and struct ntdb_file), or a
 * previously allocated pointer.  This is useful for relationship
 * tracking, such as the talloc library.
 *
 * The expand function is realloc, but only ever used to expand an
 * existing allocation.
 *
 * Be careful mixing allocators: two ntdb_contexts which have the same file
 * open will share the same struct ntdb_file.  This may be allocated by one
 * ntdb's allocator, and freed by the other.
 */
struct ntdb_attribute_allocator {
	struct ntdb_attribute_base base; /* .attr = NTDB_ATTRIBUTE_ALLOCATOR */
	void *(*alloc)(const void *owner, size_t len, void *priv_data);
	void *(*expand)(void *old, size_t newlen, void *priv_data);
	void (*free)(void *old, void *priv_data);
	void *priv_data;
};

/**
 * union ntdb_attribute - ntdb attributes.
 *
 * This represents all the known attributes.
 *
 * See also:
 *	struct ntdb_attribute_log, struct ntdb_attribute_hash,
 *	struct ntdb_attribute_seed, struct ntdb_attribute_stats,
 *	struct ntdb_attribute_openhook, struct ntdb_attribute_flock,
 *	struct ntdb_attribute_allocator alloc.
 */
union ntdb_attribute {
	struct ntdb_attribute_base base;
	struct ntdb_attribute_log log;
	struct ntdb_attribute_hash hash;
	struct ntdb_attribute_seed seed;
	struct ntdb_attribute_stats stats;
	struct ntdb_attribute_openhook openhook;
	struct ntdb_attribute_flock flock;
	struct ntdb_attribute_allocator alloc;
	struct ntdb_attribute_hashsize hashsize;
};

#ifdef  __cplusplus
}
#endif

#endif /* ntdb.h */
