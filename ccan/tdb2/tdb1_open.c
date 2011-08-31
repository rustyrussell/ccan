 /*
   Unix SMB/CIFS implementation.

   trivial database library

   Copyright (C) Andrew Tridgell              1999-2005
   Copyright (C) Paul `Rusty' Russell		   2000
   Copyright (C) Jeremy Allison			   2000-2003

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
#include <assert.h>
#include "tdb1_private.h"
#include <assert.h>

/* We use two hashes to double-check they're using the right hash function. */
void tdb1_header_hash(struct tdb_context *tdb,
		     uint32_t *magic1_hash, uint32_t *magic2_hash)
{
	uint32_t tdb1_magic = TDB1_MAGIC;

	*magic1_hash = tdb_hash(tdb, TDB_MAGIC_FOOD, sizeof(TDB_MAGIC_FOOD));
	*magic2_hash = tdb_hash(tdb, TDB1_CONV(tdb1_magic), sizeof(tdb1_magic));

	/* Make sure at least one hash is non-zero! */
	if (*magic1_hash == 0 && *magic2_hash == 0)
		*magic1_hash = 1;
}

static void tdb_context_init(struct tdb_context *tdb,
			     struct tdb_attribute_tdb1_max_dead *max_dead)
{
	assert(tdb->flags & TDB_VERSION1);

	tdb1_io_init(tdb);

	tdb->tdb1.traverse_read = tdb->tdb1.traverse_write = 0;
	memset(&tdb->tdb1.travlocks, 0, sizeof(tdb->tdb1.travlocks));
	tdb->tdb1.transaction = NULL;

	/* cache the page size */
	tdb->tdb1.page_size = getpagesize();
	if (tdb->tdb1.page_size <= 0) {
		tdb->tdb1.page_size = 0x2000;
	}

	if (max_dead) {
		tdb->tdb1.max_dead_records = max_dead->max_dead;
	} else {
		tdb->tdb1.max_dead_records = 0;
	}
}

/* initialise a new database */
enum TDB_ERROR tdb1_new_database(struct tdb_context *tdb,
				 struct tdb_attribute_tdb1_hashsize *hashsize,
				 struct tdb_attribute_tdb1_max_dead *max_dead)
{
	struct tdb1_header *newdb;
	size_t size;
	int hash_size = TDB1_DEFAULT_HASH_SIZE;
	enum TDB_ERROR ret = TDB_ERR_IO;

	tdb_context_init(tdb, max_dead);

	/* Default TDB2 hash becomes default TDB1 hash. */
	if (tdb->hash_fn == tdb_jenkins_hash)
		tdb->hash_fn = tdb1_old_hash;

	if (hashsize)
		hash_size = hashsize->hsize;

	/* We make it up in memory, then write it out if not internal */
	size = sizeof(struct tdb1_header) + (hash_size+1)*sizeof(tdb1_off_t);
	if (!(newdb = (struct tdb1_header *)calloc(size, 1))) {
		return TDB_ERR_OOM;
	}

	/* Fill in the header */
	newdb->version = TDB1_VERSION;
	newdb->hash_size = hash_size;

	tdb1_header_hash(tdb, &newdb->magic1_hash, &newdb->magic2_hash);

	/* Make sure older tdbs (which don't check the magic hash fields)
	 * will refuse to open this TDB. */
	if (tdb->hash_fn == tdb1_incompatible_hash)
		newdb->rwlocks = TDB1_HASH_RWLOCK_MAGIC;

	memcpy(&tdb->tdb1.header, newdb, sizeof(tdb->tdb1.header));
	/* This creates an endian-converted db. */
	TDB1_CONV(*newdb);
	/* Don't endian-convert the magic food! */
	memcpy(newdb->magic_food, TDB_MAGIC_FOOD, strlen(TDB_MAGIC_FOOD)+1);

	if (tdb->flags & TDB_INTERNAL) {
		tdb->file->map_size = size;
		tdb->file->map_ptr = (char *)newdb;
		return TDB_SUCCESS;
	}
	if (lseek(tdb->file->fd, 0, SEEK_SET) == -1)
		goto fail;

	if (ftruncate(tdb->file->fd, 0) == -1)
		goto fail;

	/* we still have "ret == TDB_ERR_IO" here */
	if (tdb1_write_all(tdb->file->fd, newdb, size))
		ret = TDB_SUCCESS;

  fail:
	SAFE_FREE(newdb);
	return ret;
}

typedef void (*tdb1_log_func)(struct tdb_context *, enum tdb_log_level, enum TDB_ERROR,
			      const char *, void *);
typedef uint64_t (*tdb1_hash_func)(const void *key, size_t len, uint64_t seed,
				   void *data);

struct tdb1_logging_context {
        tdb1_log_func log_fn;
        void *log_private;
};

static bool hash_correct(struct tdb_context *tdb,
			 uint32_t *m1, uint32_t *m2)
{
	/* older TDB without magic hash references */
	if (tdb->tdb1.header.magic1_hash == 0
	    && tdb->tdb1.header.magic2_hash == 0) {
		return true;
	}

	tdb1_header_hash(tdb, m1, m2);
	return (tdb->tdb1.header.magic1_hash == *m1 &&
		tdb->tdb1.header.magic2_hash == *m2);
}

static bool check_header_hash(struct tdb_context *tdb,
			      uint32_t *m1, uint32_t *m2)
{
	if (hash_correct(tdb, m1, m2))
		return true;

	/* If they use one inbuilt, try the other inbuilt hash. */
	if (tdb->hash_fn == tdb1_old_hash)
		tdb->hash_fn = tdb1_incompatible_hash;
	else if (tdb->hash_fn == tdb1_incompatible_hash)
		tdb->hash_fn = tdb1_old_hash;
	else
		return false;
	return hash_correct(tdb, m1, m2);
}

/* We are hold the TDB open lock on tdb->fd. */
enum TDB_ERROR tdb1_open(struct tdb_context *tdb,
			 struct tdb_attribute_tdb1_max_dead *max_dead)
{
	const char *hash_alg;
	uint32_t magic1, magic2;

	tdb->flags |= TDB_VERSION1;

	tdb_context_init(tdb, max_dead);

	/* Default TDB2 hash becomes default TDB1 hash. */
	if (tdb->hash_fn == tdb_jenkins_hash) {
		tdb->hash_fn = tdb1_old_hash;
		hash_alg = "default";
	} else if (tdb->hash_fn == tdb1_incompatible_hash)
		hash_alg = "tdb1_incompatible_hash";
	else
		hash_alg = "the user defined";

	if (tdb->tdb1.header.version != TDB1_BYTEREV(TDB1_VERSION)) {
		if (tdb->flags & TDB_CONVERT) {
			return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
					  "tdb1_open:"
					  " %s does not need TDB_CONVERT",
					  tdb->name);
		}
	} else {
		tdb->flags |= TDB_CONVERT;
		tdb1_convert(&tdb->tdb1.header, sizeof(tdb->tdb1.header));
	}

	if (tdb->tdb1.header.rwlocks != 0 &&
	    tdb->tdb1.header.rwlocks != TDB1_HASH_RWLOCK_MAGIC) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb1_open: spinlocks no longer supported");
	}

	if (!check_header_hash(tdb, &magic1, &magic2)) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_USE_ERROR,
			   "tdb1_open: "
			   "%s was not created with %s hash function we are using\n"
			   "magic1_hash[0x%08X %s 0x%08X] "
			   "magic2_hash[0x%08X %s 0x%08X]",
			   tdb->name, hash_alg,
			   tdb->tdb1.header.magic1_hash,
			   (tdb->tdb1.header.magic1_hash == magic1) ? "==" : "!=",
			   magic1,
			   tdb->tdb1.header.magic2_hash,
			   (tdb->tdb1.header.magic2_hash == magic2) ? "==" : "!=",
			   magic2);
	}
	return TDB_SUCCESS;
}
