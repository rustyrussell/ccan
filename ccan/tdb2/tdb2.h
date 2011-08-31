#ifndef CCAN_TDB2_H
#define CCAN_TDB2_H

/*
   TDB version 2: trivial database library

   Copyright (C) Andrew Tridgell 1999-2004
   Copyright (C) Rusty Russell 2010-2011

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

#ifdef  __cplusplus
extern "C" {
#endif

#ifndef _SAMBA_BUILD_
#include "config.h"
#if HAVE_FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
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
#include <ccan/compiler/compiler.h>
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/cast/cast.h>

union tdb_attribute;
struct tdb_context;

/**
 * tdb_open - open a database file
 * @name: the file name (can be NULL if flags contains TDB_INTERNAL)
 * @tdb_flags: options for this database
 * @open_flags: flags argument for tdb's open() call.
 * @mode: mode argument for tdb's open() call.
 * @attributes: linked list of extra attributes for this tdb.
 *
 * This call opens (and potentially creates) a database file.
 * Multiple processes can have the TDB file open at once.
 *
 * On failure it will return NULL, and set errno: it may also call
 * any log attribute found in @attributes.
 *
 * See also:
 *	union tdb_attribute
 */
struct tdb_context *tdb_open(const char *name, int tdb_flags,
			     int open_flags, mode_t mode,
			     union tdb_attribute *attributes);


/* flags for tdb_open() */
#define TDB_DEFAULT 0 /* just a readability place holder */
#define TDB_INTERNAL 2 /* don't store on disk */
#define TDB_NOLOCK   4 /* don't do any locking */
#define TDB_NOMMAP   8 /* don't use mmap */
#define TDB_CONVERT 16 /* convert endian */
#define TDB_NOSYNC   64 /* don't use synchronous transactions */
#define TDB_SEQNUM   128 /* maintain a sequence number */
#define TDB_ALLOW_NESTING   256 /* fake nested transactions */
#define TDB_RDONLY   512 /* implied by O_RDONLY */

/**
 * tdb_close - close and free a tdb.
 * @tdb: the tdb context returned from tdb_open()
 *
 * This always succeeds, in that @tdb is unusable after this call.  But if
 * some unexpected error occurred while closing, it will return non-zero
 * (the only clue as to cause will be via the log attribute).
 */
int tdb_close(struct tdb_context *tdb);

/**
 * struct tdb_data - representation of keys or values.
 * @dptr: the data pointer
 * @dsize: the size of the data pointed to by dptr.
 *
 * This is the "blob" representation of keys and data used by TDB.
 */
typedef struct tdb_data {
	unsigned char *dptr;
	size_t dsize;
} TDB_DATA;

/**
 * enum TDB_ERROR - error returns for TDB
 *
 * See Also:
 *	tdb_errorstr()
 */
enum TDB_ERROR {
	TDB_SUCCESS	= 0,	/* No error. */
	TDB_ERR_CORRUPT = -1,	/* We read the db, and it was bogus. */
	TDB_ERR_IO	= -2,	/* We couldn't read/write the db. */
	TDB_ERR_LOCK	= -3,	/* Locking failed. */
	TDB_ERR_OOM	= -4,	/* Out of Memory. */
	TDB_ERR_EXISTS	= -5,	/* The key already exists. */
	TDB_ERR_NOEXIST	= -6,	/* The key does not exist. */
	TDB_ERR_EINVAL	= -7,	/* You're using it wrong. */
	TDB_ERR_RDONLY	= -8,	/* The database is read-only. */
	TDB_ERR_LAST = TDB_ERR_RDONLY
};

/**
 * tdb_store - store a key/value pair in a tdb.
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key
 * @dbuf: the data to associate with the key.
 * @flag: TDB_REPLACE, TDB_INSERT or TDB_MODIFY.
 *
 * This inserts (or overwrites) a key/value pair in the TDB.  If flag
 * is TDB_REPLACE, it doesn't matter whether the key exists or not;
 * TDB_INSERT means it must not exist (returns TDB_ERR_EXISTS otherwise),
 * and TDB_MODIFY means it must exist (returns TDB_ERR_NOEXIST otherwise).
 *
 * On success, this returns TDB_SUCCESS.
 *
 * See also:
 *	tdb_fetch, tdb_transaction_start, tdb_append, tdb_delete.
 */
enum TDB_ERROR tdb_store(struct tdb_context *tdb,
			 struct tdb_data key,
			 struct tdb_data dbuf,
			 int flag);

/* flags to tdb_store() */
#define TDB_REPLACE 1		/* A readability place holder */
#define TDB_INSERT 2 		/* Don't overwrite an existing entry */
#define TDB_MODIFY 3		/* Don't create an existing entry    */

/**
 * tdb_fetch - fetch a value from a tdb.
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key
 * @data: pointer to data.
 *
 * This looks up a key in the database and sets it in @data.
 *
 * If it returns TDB_SUCCESS, the key was found: it is your
 * responsibility to call free() on @data->dptr.
 *
 * Otherwise, it returns an error (usually, TDB_ERR_NOEXIST) and @data is
 * undefined.
 */
enum TDB_ERROR tdb_fetch(struct tdb_context *tdb, struct tdb_data key,
			 struct tdb_data *data);

/**
 * tdb_errorstr - map the tdb error onto a constant readable string
 * @ecode: the enum TDB_ERROR to map.
 *
 * This is useful for displaying errors to users.
 */
const char *tdb_errorstr(enum TDB_ERROR ecode);

/**
 * tdb_append - append a value to a key/value pair in a tdb.
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key
 * @dbuf: the data to append.
 *
 * This is equivalent to fetching a record, reallocating .dptr to add the
 * data, and writing it back, only it's much more efficient.  If the key
 * doesn't exist, it's equivalent to tdb_store (with an additional hint that
 * you expect to expand the record in future).
 *
 * See Also:
 *	tdb_fetch(), tdb_store()
 */
enum TDB_ERROR tdb_append(struct tdb_context *tdb,
			  struct tdb_data key, struct tdb_data dbuf);

/**
 * tdb_delete - delete a key from a tdb.
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to delete.
 *
 * Returns TDB_SUCCESS on success, or an error (usually TDB_ERR_NOEXIST).
 *
 * See Also:
 *	tdb_fetch(), tdb_store()
 */
enum TDB_ERROR tdb_delete(struct tdb_context *tdb, struct tdb_data key);

/**
 * tdb_exists - does a key exist in the database?
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to search for.
 *
 * Returns true if it exists, or false if it doesn't or any other error.
 */
bool tdb_exists(struct tdb_context *tdb, TDB_DATA key);

/**
 * tdb_deq - are struct tdb_data equal?
 * @a: one struct tdb_data
 * @b: another struct tdb_data
 */
static inline bool tdb_deq(struct tdb_data a, struct tdb_data b)
{
	return a.dsize == b.dsize && memcmp(a.dptr, b.dptr, a.dsize) == 0;
}

/**
 * tdb_mkdata - make a struct tdb_data from const data
 * @p: the constant pointer
 * @len: the length
 *
 * As the dptr member of struct tdb_data is not constant, you need to
 * cast it.  This function keeps thost casts in one place, as well as
 * suppressing the warning some compilers give when casting away a
 * qualifier (eg. gcc with -Wcast-qual)
 */
static inline struct tdb_data tdb_mkdata(const void *p, size_t len)
{
	struct tdb_data d;
	d.dptr = cast_const(void *, p);
	d.dsize = len;
	return d;
}

/**
 * tdb_transaction_start - start a transaction
 * @tdb: the tdb context returned from tdb_open()
 *
 * This begins a series of atomic operations.  Other processes will be able
 * to read the tdb, but not alter it (they will block), nor will they see
 * any changes until tdb_transaction_commit() is called.
 *
 * Note that if the TDB_ALLOW_NESTING flag is set, a tdb_transaction_start()
 * within a transaction will succeed, but it's not a real transaction:
 * (1) An inner transaction which is committed is not actually committed until
 *     the outer transaction is; if the outer transaction is cancelled, the
 *     inner ones are discarded.
 * (2) tdb_transaction_cancel() marks the outer transaction as having an error,
 *     so the final tdb_transaction_commit() will fail.
 * (3) the outer transaction will see the results of the inner transaction.
 *
 * See Also:
 *	tdb_transaction_cancel, tdb_transaction_commit.
 */
enum TDB_ERROR tdb_transaction_start(struct tdb_context *tdb);

/**
 * tdb_transaction_cancel - abandon a transaction
 * @tdb: the tdb context returned from tdb_open()
 *
 * This aborts a transaction, discarding any changes which were made.
 * tdb_close() does this implicitly.
 */
void tdb_transaction_cancel(struct tdb_context *tdb);

/**
 * tdb_transaction_commit - commit a transaction
 * @tdb: the tdb context returned from tdb_open()
 *
 * This completes a transaction, writing any changes which were made.
 *
 * fsync() is used to commit the transaction (unless TDB_NOSYNC is set),
 * making it robust against machine crashes, but very slow compared to
 * other TDB operations.
 *
 * A failure can only be caused by unexpected errors (eg. I/O or
 * memory); this is no point looping on transaction failure.
 *
 * See Also:
 *	tdb_transaction_prepare_commit()
 */
enum TDB_ERROR tdb_transaction_commit(struct tdb_context *tdb);

/**
 * tdb_transaction_prepare_commit - prepare to commit a transaction
 * @tdb: the tdb context returned from tdb_open()
 *
 * This ensures we have the resources to commit a transaction (using
 * tdb_transaction_commit): if this succeeds then a transaction will only
 * fail if the write() or fsync() calls fail.
 *
 * If this fails you must still call tdb_transaction_cancel() to cancel
 * the transaction.
 *
 * See Also:
 *	tdb_transaction_commit()
 */
enum TDB_ERROR tdb_transaction_prepare_commit(struct tdb_context *tdb);

/**
 * tdb_traverse - traverse a TDB
 * @tdb: the tdb context returned from tdb_open()
 * @fn: the function to call for every key/value pair (or NULL)
 * @p: the pointer to hand to @f
 *
 * This walks the TDB until all they keys have been traversed, or @fn
 * returns non-zero.  If the traverse function or other processes are
 * changing data or adding or deleting keys, the traverse may be
 * unreliable: keys may be skipped or (rarely) visited twice.
 *
 * There is one specific exception: the special case of deleting the
 * current key does not undermine the reliability of the traversal.
 *
 * On success, returns the number of keys iterated.  On error returns
 * a negative enum TDB_ERROR value.
 */
#define tdb_traverse(tdb, fn, p)					\
	tdb_traverse_(tdb, typesafe_cb_preargs(int, void *, (fn), (p),	\
					       struct tdb_context *,	\
					       TDB_DATA, TDB_DATA), (p))

int64_t tdb_traverse_(struct tdb_context *tdb,
		      int (*fn)(struct tdb_context *,
				TDB_DATA, TDB_DATA, void *), void *p);

/**
 * tdb_parse_record - operate directly on data in the database.
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key whose record we should hand to @parse
 * @parse: the function to call for the data
 * @data: the private pointer to hand to @parse (types must match).
 *
 * This avoids a copy for many cases, by handing you a pointer into
 * the memory-mapped database.  It also locks the record to prevent
 * other accesses at the same time.
 *
 * Do not alter the data handed to parse()!
 */
#define tdb_parse_record(tdb, key, parse, data)				\
	tdb_parse_record_((tdb), (key),					\
			  typesafe_cb_preargs(enum TDB_ERROR, void *,	\
					      (parse), (data),		\
					      TDB_DATA, TDB_DATA), (data))

enum TDB_ERROR tdb_parse_record_(struct tdb_context *tdb,
				 TDB_DATA key,
				 enum TDB_ERROR (*parse)(TDB_DATA k,
							 TDB_DATA d,
							 void *data),
				 void *data);

/**
 * tdb_get_seqnum - get a database sequence number
 * @tdb: the tdb context returned from tdb_open()
 *
 * This returns a sequence number: any change to the database from a
 * tdb context opened with the TDB_SEQNUM flag will cause that number
 * to increment.  Note that the incrementing is unreliable (it is done
 * without locking), so this is only useful as an optimization.
 *
 * For example, you may have a regular database backup routine which
 * does not operate if the sequence number is unchanged.  In the
 * unlikely event of a failed increment, it will be backed up next
 * time any way.
 *
 * Returns an enum TDB_ERROR (ie. negative) on error.
 */
int64_t tdb_get_seqnum(struct tdb_context *tdb);

/**
 * tdb_firstkey - get the "first" key in a TDB
 * @tdb: the tdb context returned from tdb_open()
 * @key: pointer to key.
 *
 * This returns an arbitrary key in the database; with tdb_nextkey() it allows
 * open-coded traversal of the database, though it is slightly less efficient
 * than tdb_traverse.
 *
 * It is your responsibility to free @key->dptr on success.
 *
 * Returns TDB_ERR_NOEXIST if the database is empty.
 */
enum TDB_ERROR tdb_firstkey(struct tdb_context *tdb, struct tdb_data *key);

/**
 * tdb_nextkey - get the "next" key in a TDB
 * @tdb: the tdb context returned from tdb_open()
 * @key: a key returned by tdb_firstkey() or tdb_nextkey().
 *
 * This returns another key in the database; it will free @key.dptr for
 * your convenience.
 *
 * Returns TDB_ERR_NOEXIST if there are no more keys.
 */
enum TDB_ERROR tdb_nextkey(struct tdb_context *tdb, struct tdb_data *key);

/**
 * tdb_chainlock - lock a record in the TDB
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to lock.
 *
 * This prevents any access occurring to a group of keys including @key,
 * even if @key does not exist.  This allows primitive atomic updates of
 * records without using transactions.
 *
 * You cannot begin a transaction while holding a tdb_chainlock(), nor can
 * you do any operations on any other keys in the database.  This also means
 * that you cannot hold more than one tdb_chainlock() at a time.
 *
 * See Also:
 *	tdb_chainunlock()
 */
enum TDB_ERROR tdb_chainlock(struct tdb_context *tdb, TDB_DATA key);

/**
 * tdb_chainunlock - unlock a record in the TDB
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to unlock.
 *
 * The key must have previously been locked by tdb_chainlock().
 */
void tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key);

/**
 * tdb_chainlock_read - lock a record in the TDB, for reading
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to lock.
 *
 * This prevents any changes from occurring to a group of keys including @key,
 * even if @key does not exist.  This allows primitive atomic updates of
 * records without using transactions.
 *
 * You cannot begin a transaction while holding a tdb_chainlock_read(), nor can
 * you do any operations on any other keys in the database.  This also means
 * that you cannot hold more than one tdb_chainlock()/read() at a time.
 *
 * See Also:
 *	tdb_chainlock()
 */
enum TDB_ERROR tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key);

/**
 * tdb_chainunlock_read - unlock a record in the TDB for reading
 * @tdb: the tdb context returned from tdb_open()
 * @key: the key to unlock.
 *
 * The key must have previously been locked by tdb_chainlock_read().
 */
void tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key);

/**
 * tdb_lockall - lock the entire TDB
 * @tdb: the tdb context returned from tdb_open()
 *
 * You cannot hold a tdb_chainlock while calling this.  It nests, so you
 * must call tdb_unlockall as many times as you call tdb_lockall.
 */
enum TDB_ERROR tdb_lockall(struct tdb_context *tdb);

/**
 * tdb_unlockall - unlock the entire TDB
 * @tdb: the tdb context returned from tdb_open()
 */
void tdb_unlockall(struct tdb_context *tdb);

/**
 * tdb_lockall_read - lock the entire TDB for reading
 * @tdb: the tdb context returned from tdb_open()
 *
 * This prevents others writing to the database, eg. tdb_delete, tdb_store,
 * tdb_append, but not tdb_fetch.
 *
 * You cannot hold a tdb_chainlock while calling this.  It nests, so you
 * must call tdb_unlockall_read as many times as you call tdb_lockall_read.
 */
enum TDB_ERROR tdb_lockall_read(struct tdb_context *tdb);

/**
 * tdb_unlockall_read - unlock the entire TDB for reading
 * @tdb: the tdb context returned from tdb_open()
 */
void tdb_unlockall_read(struct tdb_context *tdb);

/**
 * tdb_wipe_all - wipe the database clean
 * @tdb: the tdb context returned from tdb_open()
 *
 * Completely erase the database.  This is faster than iterating through
 * each key and doing tdb_delete.
 */
enum TDB_ERROR tdb_wipe_all(struct tdb_context *tdb);

/**
 * tdb_check - check a TDB for consistency
 * @tdb: the tdb context returned from tdb_open()
 * @check: function to check each key/data pair (or NULL)
 * @data: argument for @check, must match type.
 *
 * This performs a consistency check of the open database, optionally calling
 * a check() function on each record so you can do your own data consistency
 * checks as well.  If check() returns an error, that is returned from
 * tdb_check().
 *
 * Returns TDB_SUCCESS or an error.
 */
#define tdb_check(tdb, check, data)					\
	tdb_check_((tdb), typesafe_cb_preargs(enum TDB_ERROR, void *,	\
					      (check), (data),		\
					      struct tdb_data,		\
					      struct tdb_data),		\
		   (data))

enum TDB_ERROR tdb_check_(struct tdb_context *tdb,
			  enum TDB_ERROR (*check)(struct tdb_data k,
						  struct tdb_data d,
						  void *data),
			  void *data);

/**
 * tdb_error - get the last error (not threadsafe)
 * @tdb: the tdb context returned from tdb_open()
 *
 * Returns the last error returned by a TDB function.
 *
 * This makes porting from TDB1 easier, but note that the last error is not
 * reliable in threaded programs.
 */
enum TDB_ERROR tdb_error(struct tdb_context *tdb);

/**
 * enum tdb_summary_flags - flags for tdb_summary.
 */
enum tdb_summary_flags {
	TDB_SUMMARY_HISTOGRAMS = 1 /* Draw graphs in the summary. */
};

/**
 * tdb_summary - return a string describing the TDB state
 * @tdb: the tdb context returned from tdb_open()
 * @flags: flags to control the summary output.
 * @summary: pointer to string to allocate.
 *
 * This returns a developer-readable string describing the overall
 * state of the tdb, such as the percentage used and sizes of records.
 * It is designed to provide information about the tdb at a glance
 * without displaying any keys or data in the database.
 *
 * On success, sets @summary to point to a malloc()'ed nul-terminated
 * multi-line string.  It is your responsibility to free() it.
 */
enum TDB_ERROR tdb_summary(struct tdb_context *tdb,
			   enum tdb_summary_flags flags,
			   char **summary);

 
/**
 * tdb_get_flags - return the flags for a tdb
 * @tdb: the tdb context returned from tdb_open()
 *
 * This returns the flags on the current tdb.  Some of these are caused by
 * the flags argument to tdb_open(), others (such as TDB_CONVERT) are
 * intuited.
 */
unsigned int tdb_get_flags(struct tdb_context *tdb);

/**
 * tdb_add_flag - set a flag for a tdb
 * @tdb: the tdb context returned from tdb_open()
 * @flag: one of TDB_NOLOCK, TDB_NOMMAP, TDB_NOSYNC or TDB_ALLOW_NESTING.
 *
 * You can use this to set a flag on the TDB.  You cannot set these flags
 * on a TDB_INTERNAL tdb.
 */
void tdb_add_flag(struct tdb_context *tdb, unsigned flag);

/**
 * tdb_remove_flag - unset a flag for a tdb
 * @tdb: the tdb context returned from tdb_open()
 * @flag: one of TDB_NOLOCK, TDB_NOMMAP, TDB_NOSYNC or TDB_ALLOW_NESTING.
 *
 * You can use this to clear a flag on the TDB.  You cannot clear flags
 * on a TDB_INTERNAL tdb.
 */
void tdb_remove_flag(struct tdb_context *tdb, unsigned flag);

/**
 * enum tdb_attribute_type - descriminator for union tdb_attribute.
 */
enum tdb_attribute_type {
	TDB_ATTRIBUTE_LOG = 0,
	TDB_ATTRIBUTE_HASH = 1,
	TDB_ATTRIBUTE_SEED = 2,
	TDB_ATTRIBUTE_STATS = 3,
	TDB_ATTRIBUTE_OPENHOOK = 4,
	TDB_ATTRIBUTE_FLOCK = 5
};

/**
 * tdb_get_attribute - get an attribute for an existing tdb
 * @tdb: the tdb context returned from tdb_open()
 * @attr: the union tdb_attribute to set.
 *
 * This gets an attribute from a TDB which has previously been set (or
 * may return the default values).  Set @attr.base.attr to the
 * attribute type you want get.
 */
enum TDB_ERROR tdb_get_attribute(struct tdb_context *tdb,
				 union tdb_attribute *attr);

/**
 * tdb_set_attribute - set an attribute for an existing tdb
 * @tdb: the tdb context returned from tdb_open()
 * @attr: the union tdb_attribute to set.
 *
 * This sets an attribute on a TDB, overriding any previous attribute
 * of the same type.  It returns TDB_ERR_EINVAL if the attribute is
 * unknown or invalid.
 *
 * Note that TDB_ATTRIBUTE_HASH, TDB_ATTRIBUTE_SEED and
 * TDB_ATTRIBUTE_OPENHOOK cannot currently be set after tdb_open.
 */
enum TDB_ERROR tdb_set_attribute(struct tdb_context *tdb,
				 const union tdb_attribute *attr);

/**
 * tdb_unset_attribute - reset an attribute for an existing tdb
 * @tdb: the tdb context returned from tdb_open()
 * @type: the attribute type to unset.
 *
 * This unsets an attribute on a TDB, returning it to the defaults
 * (where applicable).
 *
 * Note that it only makes sense for TDB_ATTRIBUTE_LOG and TDB_ATTRIBUTE_FLOCK
 * to be unset.
 */
void tdb_unset_attribute(struct tdb_context *tdb,
			 enum tdb_attribute_type type);

/**
 * tdb_name - get the name of a tdb
 * @tdb: the tdb context returned from tdb_open()
 *
 * This returns a copy of the name string, made at tdb_open() time.  If that
 * argument was NULL (possible for a TDB_INTERNAL db) this will return NULL.
 *
 * This is mostly useful for logging.
 */
const char *tdb_name(const struct tdb_context *tdb);

/**
 * tdb_fd - get the file descriptor of a tdb
 * @tdb: the tdb context returned from tdb_open()
 *
 * This returns the file descriptor for the underlying database file, or -1
 * for TDB_INTERNAL.
 */
int tdb_fd(const struct tdb_context *tdb);

/**
 * tdb_foreach - iterate through every open TDB.
 * @fn: the function to call for every TDB
 * @p: the pointer to hand to @fn
 *
 * TDB internally keeps track of all open TDBs; this function allows you to
 * iterate through them.  If @fn returns non-zero, traversal stops.
 */
#define tdb_foreach(fn, p)						\
	tdb_foreach_(typesafe_cb_preargs(int, void *, (fn), (p),	\
					 struct tdb_context *), (p))

void tdb_foreach_(int (*fn)(struct tdb_context *, void *), void *p);

/**
 * struct tdb_attribute_base - common fields for all tdb attributes.
 */
struct tdb_attribute_base {
	enum tdb_attribute_type attr;
	union tdb_attribute *next;
};

/**
 * enum tdb_log_level - log levels for tdb_attribute_log
 * @TDB_LOG_ERROR: used to log unrecoverable errors such as I/O errors
 *		   or internal consistency failures.
 * @TDB_LOG_USE_ERROR: used to log usage errors such as invalid parameters
 *		   or writing to a read-only database.
 * @TDB_LOG_WARNING: used for informational messages on issues which
 *		     are unusual but handled by TDB internally, such
 *		     as a failure to mmap or failure to open /dev/urandom.
 */
enum tdb_log_level {
	TDB_LOG_ERROR,
	TDB_LOG_USE_ERROR,
	TDB_LOG_WARNING
};

/**
 * struct tdb_attribute_log - log function attribute
 *
 * This attribute provides a hook for you to log errors.
 */
struct tdb_attribute_log {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_LOG */
	void (*fn)(struct tdb_context *tdb,
		   enum tdb_log_level level,
		   enum TDB_ERROR ecode,
		   const char *message,
		   void *data);
	void *data;
};

/**
 * struct tdb_attribute_hash - hash function attribute
 *
 * This attribute allows you to provide an alternative hash function.
 * This hash function will be handed keys from the database; it will also
 * be handed the 8-byte TDB_HASH_MAGIC value for checking the header (the
 * tdb_open() will fail if the hash value doesn't match the header).
 *
 * Note that if your hash function gives different results on
 * different machine endians, your tdb will no longer work across
 * different architectures!
 */
struct tdb_attribute_hash {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_HASH */
	uint64_t (*fn)(const void *key, size_t len, uint64_t seed,
		       void *data);
	void *data;
};

/**
 * struct tdb_attribute_seed - hash function seed attribute
 *
 * The hash function seed is normally taken from /dev/urandom (or equivalent)
 * but can be set manually here.  This is mainly for testing purposes.
 */
struct tdb_attribute_seed {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_SEED */
	uint64_t seed;
};

/**
 * struct tdb_attribute_stats - tdb operational statistics
 *
 * This attribute records statistics of various low-level TDB operations.
 * This can be used to assist performance evaluation.  This is only
 * useful for tdb_get_attribute().
 *
 * New fields will be added at the end, hence the "size" argument which
 * indicates how large your structure is: it must be filled in before
 * calling tdb_get_attribute(), which will overwrite it with the size
 * tdb knows about.
 */
struct tdb_attribute_stats {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_STATS */
	size_t size; /* = sizeof(struct tdb_attribute_stats) */
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
	uint64_t   compare_wrong_bucket;
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
	uint64_t expands;
	uint64_t frees;
	uint64_t locks;
	uint64_t   lock_lowlevel;
	uint64_t   lock_nonblock;
	uint64_t     lock_nonblock_fail;
};

/**
 * struct tdb_attribute_openhook - tdb special effects hook for open
 *
 * This attribute contains a function to call once we have the OPEN_LOCK
 * for the tdb, but before we've examined its contents.  If this succeeds,
 * the tdb will be populated if it's then zero-length.
 *
 * This is a hack to allow support for TDB1-style TDB_CLEAR_IF_FIRST
 * behaviour.
 */
struct tdb_attribute_openhook {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_OPENHOOK */
	enum TDB_ERROR (*fn)(int fd, void *data);
	void *data;
};

/**
 * struct tdb_attribute_flock - tdb special effects hook for file locking
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
struct tdb_attribute_flock {
	struct tdb_attribute_base base; /* .attr = TDB_ATTRIBUTE_FLOCK */
	int (*lock)(int fd,int rw, off_t off, off_t len, bool waitflag, void *);
	int (*unlock)(int fd, int rw, off_t off, off_t len, void *);
	void *data;
};

/**
 * union tdb_attribute - tdb attributes.
 *
 * This represents all the known attributes.
 *
 * See also:
 *	struct tdb_attribute_log, struct tdb_attribute_hash,
 *	struct tdb_attribute_seed, struct tdb_attribute_stats,
 *	struct tdb_attribute_openhook, struct tdb_attribute_flock.
 */
union tdb_attribute {
	struct tdb_attribute_base base;
	struct tdb_attribute_log log;
	struct tdb_attribute_hash hash;
	struct tdb_attribute_seed seed;
	struct tdb_attribute_stats stats;
	struct tdb_attribute_openhook openhook;
	struct tdb_attribute_flock flock;
};

#ifdef  __cplusplus
}
#endif

#endif /* tdb2.h */
