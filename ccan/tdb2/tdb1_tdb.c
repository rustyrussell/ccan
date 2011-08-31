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

#include "tdb1_private.h"

TDB_DATA tdb1_null;

/*
  non-blocking increment of the tdb sequence number if the tdb has been opened using
  the TDB_SEQNUM flag
*/
void tdb1_increment_seqnum_nonblock(struct tdb1_context *tdb)
{
	tdb1_off_t seqnum=0;

	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	/* we ignore errors from this, as we have no sane way of
	   dealing with them.
	*/
	tdb1_ofs_read(tdb, TDB1_SEQNUM_OFS, &seqnum);
	seqnum++;
	tdb1_ofs_write(tdb, TDB1_SEQNUM_OFS, &seqnum);
}

/*
  increment the tdb sequence number if the tdb has been opened using
  the TDB_SEQNUM flag
*/
static void tdb1_increment_seqnum(struct tdb1_context *tdb)
{
	if (!(tdb->flags & TDB_SEQNUM)) {
		return;
	}

	if (tdb1_nest_lock(tdb, TDB1_SEQNUM_OFS, F_WRLCK,
			   TDB_LOCK_WAIT|TDB_LOCK_PROBE) != 0) {
		return;
	}

	tdb1_increment_seqnum_nonblock(tdb);

	tdb1_nest_unlock(tdb, TDB1_SEQNUM_OFS, F_WRLCK);
}

static int tdb1_key_compare(TDB_DATA key, TDB_DATA data, void *private_data)
{
	return memcmp(data.dptr, key.dptr, data.dsize);
}

/* Returns 0 on fail.  On success, return offset of record, and fills
   in rec */
static tdb1_off_t tdb1_find(struct tdb1_context *tdb, TDB_DATA key, uint32_t hash,
			struct tdb1_record *r)
{
	tdb1_off_t rec_ptr;

	/* read in the hash top */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (tdb1_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (!TDB1_DEAD(r) && hash==r->full_hash
		    && key.dsize==r->key_len
		    && tdb1_parse_data(tdb, key, rec_ptr + sizeof(*r),
				      r->key_len, tdb1_key_compare,
				      NULL) == 0) {
			return rec_ptr;
		}
		/* detect tight infinite loop */
		if (rec_ptr == r->next) {
			tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						TDB_LOG_ERROR,
						"tdb1_find: loop detected.");
			return 0;
		}
		rec_ptr = r->next;
	}
	tdb->last_error = TDB_ERR_NOEXIST;
	return 0;
}

/* As tdb1_find, but if you succeed, keep the lock */
tdb1_off_t tdb1_find_lock_hash(struct tdb1_context *tdb, TDB_DATA key, uint32_t hash, int locktype,
			   struct tdb1_record *rec)
{
	uint32_t rec_ptr;

	if (tdb1_lock(tdb, TDB1_BUCKET(hash), locktype) == -1)
		return 0;
	if (!(rec_ptr = tdb1_find(tdb, key, hash, rec)))
		tdb1_unlock(tdb, TDB1_BUCKET(hash), locktype);
	return rec_ptr;
}

static TDB_DATA _tdb1_fetch(struct tdb1_context *tdb, TDB_DATA key);

/* update an entry in place - this only works if the new data size
   is <= the old data size and the key exists.
   on failure return -1.
*/
static int tdb1_update_hash(struct tdb1_context *tdb, TDB_DATA key, uint32_t hash, TDB_DATA dbuf)
{
	struct tdb1_record rec;
	tdb1_off_t rec_ptr;

	/* find entry */
	if (!(rec_ptr = tdb1_find(tdb, key, hash, &rec)))
		return -1;

	/* it could be an exact duplicate of what is there - this is
	 * surprisingly common (eg. with a ldb re-index). */
	if (rec.key_len == key.dsize &&
	    rec.data_len == dbuf.dsize &&
	    rec.full_hash == hash) {
		TDB_DATA data = _tdb1_fetch(tdb, key);
		if (data.dsize == dbuf.dsize &&
		    memcmp(data.dptr, dbuf.dptr, data.dsize) == 0) {
			if (data.dptr) {
				free(data.dptr);
			}
			return 0;
		}
		if (data.dptr) {
			free(data.dptr);
		}
	}

	/* must be long enough key, data and tailer */
	if (rec.rec_len < key.dsize + dbuf.dsize + sizeof(tdb1_off_t)) {
		tdb->last_error = TDB_SUCCESS; /* Not really an error */
		return -1;
	}

	if (tdb->methods->tdb1_write(tdb, rec_ptr + sizeof(rec) + rec.key_len,
		      dbuf.dptr, dbuf.dsize) == -1)
		return -1;

	if (dbuf.dsize != rec.data_len) {
		/* update size */
		rec.data_len = dbuf.dsize;
		return tdb1_rec_write(tdb, rec_ptr, &rec);
	}

	return 0;
}

/* find an entry in the database given a key */
/* If an entry doesn't exist tdb1_err will be set to
 * TDB_ERR_NOEXIST. If a key has no data attached
 * then the TDB_DATA will have zero length but
 * a non-zero pointer
 */
static TDB_DATA _tdb1_fetch(struct tdb1_context *tdb, TDB_DATA key)
{
	tdb1_off_t rec_ptr;
	struct tdb1_record rec;
	TDB_DATA ret;
	uint32_t hash;

	/* find which hash bucket it is in */
	hash = tdb_hash(tdb, key.dptr, key.dsize);
	if (!(rec_ptr = tdb1_find_lock_hash(tdb,key,hash,F_RDLCK,&rec)))
		return tdb1_null;

	ret.dptr = tdb1_alloc_read(tdb, rec_ptr + sizeof(rec) + rec.key_len,
				  rec.data_len);
	ret.dsize = rec.data_len;
	tdb1_unlock(tdb, TDB1_BUCKET(rec.full_hash), F_RDLCK);
	return ret;
}

TDB_DATA tdb1_fetch(struct tdb1_context *tdb, TDB_DATA key)
{
	TDB_DATA ret = _tdb1_fetch(tdb, key);

	return ret;
}

/*
 * Find an entry in the database and hand the record's data to a parsing
 * function. The parsing function is executed under the chain read lock, so it
 * should be fast and should not block on other syscalls.
 *
 * DON'T CALL OTHER TDB CALLS FROM THE PARSER, THIS MIGHT LEAD TO SEGFAULTS.
 *
 * For mmapped tdb's that do not have a transaction open it points the parsing
 * function directly at the mmap area, it avoids the malloc/memcpy in this
 * case. If a transaction is open or no mmap is available, it has to do
 * malloc/read/parse/free.
 *
 * This is interesting for all readers of potentially large data structures in
 * the tdb records, ldb indexes being one example.
 *
 * Return -1 if the record was not found.
 */

int tdb1_parse_record(struct tdb1_context *tdb, TDB_DATA key,
		     int (*parser)(TDB_DATA key, TDB_DATA data,
				   void *private_data),
		     void *private_data)
{
	tdb1_off_t rec_ptr;
	struct tdb1_record rec;
	int ret;
	uint32_t hash;

	/* find which hash bucket it is in */
	hash = tdb_hash(tdb, key.dptr, key.dsize);

	if (!(rec_ptr = tdb1_find_lock_hash(tdb,key,hash,F_RDLCK,&rec))) {
		/* record not found */
		tdb->last_error = TDB_ERR_NOEXIST;
		return -1;
	}

	ret = tdb1_parse_data(tdb, key, rec_ptr + sizeof(rec) + rec.key_len,
			     rec.data_len, parser, private_data);

	tdb1_unlock(tdb, TDB1_BUCKET(rec.full_hash), F_RDLCK);

	return ret;
}

/* check if an entry in the database exists

   note that 1 is returned if the key is found and 0 is returned if not found
   this doesn't match the conventions in the rest of this module, but is
   compatible with gdbm
*/
static int tdb1_exists_hash(struct tdb1_context *tdb, TDB_DATA key, uint32_t hash)
{
	struct tdb1_record rec;

	if (tdb1_find_lock_hash(tdb, key, hash, F_RDLCK, &rec) == 0)
		return 0;
	tdb1_unlock(tdb, TDB1_BUCKET(rec.full_hash), F_RDLCK);
	return 1;
}

int tdb1_exists(struct tdb1_context *tdb, TDB_DATA key)
{
	uint32_t hash = tdb_hash(tdb, key.dptr, key.dsize);
	int ret;

	ret = tdb1_exists_hash(tdb, key, hash);
	return ret;
}

/* actually delete an entry in the database given the offset */
int tdb1_do_delete(struct tdb1_context *tdb, tdb1_off_t rec_ptr, struct tdb1_record *rec)
{
	tdb1_off_t last_ptr, i;
	struct tdb1_record lastrec;

	if ((tdb->flags & TDB_RDONLY) || tdb->traverse_read) return -1;

	if (((tdb->traverse_write != 0) && (!TDB1_DEAD(rec))) ||
	    tdb1_write_lock_record(tdb, rec_ptr) == -1) {
		/* Someone traversing here: mark it as dead */
		rec->magic = TDB1_DEAD_MAGIC;
		return tdb1_rec_write(tdb, rec_ptr, rec);
	}
	if (tdb1_write_unlock_record(tdb, rec_ptr) != 0)
		return -1;

	/* find previous record in hash chain */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(rec->full_hash), &i) == -1)
		return -1;
	for (last_ptr = 0; i != rec_ptr; last_ptr = i, i = lastrec.next)
		if (tdb1_rec_read(tdb, i, &lastrec) == -1)
			return -1;

	/* unlink it: next ptr is at start of record. */
	if (last_ptr == 0)
		last_ptr = TDB1_HASH_TOP(rec->full_hash);
	if (tdb1_ofs_write(tdb, last_ptr, &rec->next) == -1)
		return -1;

	/* recover the space */
	if (tdb1_free(tdb, rec_ptr, rec) == -1)
		return -1;
	return 0;
}

static int tdb1_count_dead(struct tdb1_context *tdb, uint32_t hash)
{
	int res = 0;
	tdb1_off_t rec_ptr;
	struct tdb1_record rec;

	/* read in the hash top */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	while (rec_ptr) {
		if (tdb1_rec_read(tdb, rec_ptr, &rec) == -1)
			return 0;

		if (rec.magic == TDB1_DEAD_MAGIC) {
			res += 1;
		}
		rec_ptr = rec.next;
	}
	return res;
}

/*
 * Purge all DEAD records from a hash chain
 */
static int tdb1_purge_dead(struct tdb1_context *tdb, uint32_t hash)
{
	int res = -1;
	struct tdb1_record rec;
	tdb1_off_t rec_ptr;

	if (tdb1_lock(tdb, -1, F_WRLCK) == -1) {
		return -1;
	}

	/* read in the hash top */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(hash), &rec_ptr) == -1)
		goto fail;

	while (rec_ptr) {
		tdb1_off_t next;

		if (tdb1_rec_read(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}

		next = rec.next;

		if (rec.magic == TDB1_DEAD_MAGIC
		    && tdb1_do_delete(tdb, rec_ptr, &rec) == -1) {
			goto fail;
		}
		rec_ptr = next;
	}
	res = 0;
 fail:
	tdb1_unlock(tdb, -1, F_WRLCK);
	return res;
}

/* delete an entry in the database given a key */
static int tdb1_delete_hash(struct tdb1_context *tdb, TDB_DATA key, uint32_t hash)
{
	tdb1_off_t rec_ptr;
	struct tdb1_record rec;
	int ret;

	if (tdb->max_dead_records != 0) {

		/*
		 * Allow for some dead records per hash chain, mainly for
		 * tdb's with a very high create/delete rate like locking.tdb.
		 */

		if (tdb1_lock(tdb, TDB1_BUCKET(hash), F_WRLCK) == -1)
			return -1;

		if (tdb1_count_dead(tdb, hash) >= tdb->max_dead_records) {
			/*
			 * Don't let the per-chain freelist grow too large,
			 * delete all existing dead records
			 */
			tdb1_purge_dead(tdb, hash);
		}

		if (!(rec_ptr = tdb1_find(tdb, key, hash, &rec))) {
			tdb1_unlock(tdb, TDB1_BUCKET(hash), F_WRLCK);
			return -1;
		}

		/*
		 * Just mark the record as dead.
		 */
		rec.magic = TDB1_DEAD_MAGIC;
		ret = tdb1_rec_write(tdb, rec_ptr, &rec);
	}
	else {
		if (!(rec_ptr = tdb1_find_lock_hash(tdb, key, hash, F_WRLCK,
						   &rec)))
			return -1;

		ret = tdb1_do_delete(tdb, rec_ptr, &rec);
	}

	if (ret == 0) {
		tdb1_increment_seqnum(tdb);
	}

	if (tdb1_unlock(tdb, TDB1_BUCKET(rec.full_hash), F_WRLCK) != 0)
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_delete: WARNING tdb1_unlock failed!");
	return ret;
}

int tdb1_delete(struct tdb1_context *tdb, TDB_DATA key)
{
	uint32_t hash = tdb_hash(tdb, key.dptr, key.dsize);
	int ret;

	ret = tdb1_delete_hash(tdb, key, hash);
	return ret;
}

/*
 * See if we have a dead record around with enough space
 */
static tdb1_off_t tdb1_find_dead(struct tdb1_context *tdb, uint32_t hash,
			       struct tdb1_record *r, tdb1_len_t length)
{
	tdb1_off_t rec_ptr;

	/* read in the hash top */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(hash), &rec_ptr) == -1)
		return 0;

	/* keep looking until we find the right record */
	while (rec_ptr) {
		if (tdb1_rec_read(tdb, rec_ptr, r) == -1)
			return 0;

		if (TDB1_DEAD(r) && r->rec_len >= length) {
			/*
			 * First fit for simple coding, TODO: change to best
			 * fit
			 */
			return rec_ptr;
		}
		rec_ptr = r->next;
	}
	return 0;
}

static int _tdb1_store(struct tdb1_context *tdb, TDB_DATA key,
		       TDB_DATA dbuf, int flag, uint32_t hash)
{
	struct tdb1_record rec;
	tdb1_off_t rec_ptr;
	char *p = NULL;
	int ret = -1;

	/* check for it existing, on insert. */
	if (flag == TDB_INSERT) {
		if (tdb1_exists_hash(tdb, key, hash)) {
			tdb->last_error = TDB_ERR_EXISTS;
			goto fail;
		}
	} else {
		/* first try in-place update, on modify or replace. */
		if (tdb1_update_hash(tdb, key, hash, dbuf) == 0) {
			goto done;
		}
		if (tdb->last_error == TDB_ERR_NOEXIST &&
		    flag == TDB_MODIFY) {
			/* if the record doesn't exist and we are in TDB1_MODIFY mode then
			 we should fail the store */
			goto fail;
		}
	}
	/* reset the error code potentially set by the tdb1_update() */
	tdb->last_error = TDB_SUCCESS;

	/* delete any existing record - if it doesn't exist we don't
           care.  Doing this first reduces fragmentation, and avoids
           coalescing with `allocated' block before it's updated. */
	if (flag != TDB_INSERT)
		tdb1_delete_hash(tdb, key, hash);

	/* Copy key+value *before* allocating free space in case malloc
	   fails and we are left with a dead spot in the tdb. */

	if (!(p = (char *)malloc(key.dsize + dbuf.dsize))) {
		tdb->last_error = TDB_ERR_OOM;
		goto fail;
	}

	memcpy(p, key.dptr, key.dsize);
	if (dbuf.dsize)
		memcpy(p+key.dsize, dbuf.dptr, dbuf.dsize);

	if (tdb->max_dead_records != 0) {
		/*
		 * Allow for some dead records per hash chain, look if we can
		 * find one that can hold the new record. We need enough space
		 * for key, data and tailer. If we find one, we don't have to
		 * consult the central freelist.
		 */
		rec_ptr = tdb1_find_dead(
			tdb, hash, &rec,
			key.dsize + dbuf.dsize + sizeof(tdb1_off_t));

		if (rec_ptr != 0) {
			rec.key_len = key.dsize;
			rec.data_len = dbuf.dsize;
			rec.full_hash = hash;
			rec.magic = TDB1_MAGIC;
			if (tdb1_rec_write(tdb, rec_ptr, &rec) == -1
			    || tdb->methods->tdb1_write(
				    tdb, rec_ptr + sizeof(rec),
				    p, key.dsize + dbuf.dsize) == -1) {
				goto fail;
			}
			goto done;
		}
	}

	/*
	 * We have to allocate some space from the freelist, so this means we
	 * have to lock it. Use the chance to purge all the DEAD records from
	 * the hash chain under the freelist lock.
	 */

	if (tdb1_lock(tdb, -1, F_WRLCK) == -1) {
		goto fail;
	}

	if ((tdb->max_dead_records != 0)
	    && (tdb1_purge_dead(tdb, hash) == -1)) {
		tdb1_unlock(tdb, -1, F_WRLCK);
		goto fail;
	}

	/* we have to allocate some space */
	rec_ptr = tdb1_allocate(tdb, key.dsize + dbuf.dsize, &rec);

	tdb1_unlock(tdb, -1, F_WRLCK);

	if (rec_ptr == 0) {
		goto fail;
	}

	/* Read hash top into next ptr */
	if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(hash), &rec.next) == -1)
		goto fail;

	rec.key_len = key.dsize;
	rec.data_len = dbuf.dsize;
	rec.full_hash = hash;
	rec.magic = TDB1_MAGIC;

	/* write out and point the top of the hash chain at it */
	if (tdb1_rec_write(tdb, rec_ptr, &rec) == -1
	    || tdb->methods->tdb1_write(tdb, rec_ptr+sizeof(rec), p, key.dsize+dbuf.dsize)==-1
	    || tdb1_ofs_write(tdb, TDB1_HASH_TOP(hash), &rec_ptr) == -1) {
		/* Need to tdb1_unallocate() here */
		goto fail;
	}

 done:
	ret = 0;
 fail:
	if (ret == 0) {
		tdb1_increment_seqnum(tdb);
	}

	SAFE_FREE(p);
	return ret;
}

/* store an element in the database, replacing any existing element
   with the same key

   return 0 on success, -1 on failure
*/
int tdb1_store(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA dbuf, int flag)
{
	uint32_t hash;
	int ret;

	if ((tdb->flags & TDB_RDONLY) || tdb->traverse_read) {
		tdb->last_error = TDB_ERR_RDONLY;
		return -1;
	}

	/* find which hash bucket it is in */
	hash = tdb_hash(tdb, key.dptr, key.dsize);
	if (tdb1_lock(tdb, TDB1_BUCKET(hash), F_WRLCK) == -1)
		return -1;

	ret = _tdb1_store(tdb, key, dbuf, flag, hash);
	tdb1_unlock(tdb, TDB1_BUCKET(hash), F_WRLCK);
	return ret;
}

/* Append to an entry. Create if not exist. */
int tdb1_append(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA new_dbuf)
{
	uint32_t hash;
	TDB_DATA dbuf;
	int ret = -1;

	/* find which hash bucket it is in */
	hash = tdb_hash(tdb, key.dptr, key.dsize);
	if (tdb1_lock(tdb, TDB1_BUCKET(hash), F_WRLCK) == -1)
		return -1;

	dbuf = _tdb1_fetch(tdb, key);

	if (dbuf.dptr == NULL) {
		dbuf.dptr = (unsigned char *)malloc(new_dbuf.dsize);
	} else {
		unsigned int new_len = dbuf.dsize + new_dbuf.dsize;
		unsigned char *new_dptr;

		/* realloc '0' is special: don't do that. */
		if (new_len == 0)
			new_len = 1;
		new_dptr = (unsigned char *)realloc(dbuf.dptr, new_len);
		if (new_dptr == NULL) {
			free(dbuf.dptr);
		}
		dbuf.dptr = new_dptr;
	}

	if (dbuf.dptr == NULL) {
		tdb->last_error = TDB_ERR_OOM;
		goto failed;
	}

	memcpy(dbuf.dptr + dbuf.dsize, new_dbuf.dptr, new_dbuf.dsize);
	dbuf.dsize += new_dbuf.dsize;

	ret = _tdb1_store(tdb, key, dbuf, 0, hash);

failed:
	tdb1_unlock(tdb, TDB1_BUCKET(hash), F_WRLCK);
	SAFE_FREE(dbuf.dptr);
	return ret;
}


/*
  get the tdb sequence number. Only makes sense if the writers opened
  with TDB1_SEQNUM set. Note that this sequence number will wrap quite
  quickly, so it should only be used for a 'has something changed'
  test, not for code that relies on the count of the number of changes
  made. If you want a counter then use a tdb record.

  The aim of this sequence number is to allow for a very lightweight
  test of a possible tdb change.
*/
int tdb1_get_seqnum(struct tdb1_context *tdb)
{
	tdb1_off_t seqnum=0;

	tdb1_ofs_read(tdb, TDB1_SEQNUM_OFS, &seqnum);
	return seqnum;
}

int tdb1_hash_size(struct tdb1_context *tdb)
{
	return tdb->header.hash_size;
}


/*
  add a region of the file to the freelist. Length is the size of the region in bytes,
  which includes the free list header that needs to be added
 */
static int tdb1_free_region(struct tdb1_context *tdb, tdb1_off_t offset, ssize_t length)
{
	struct tdb1_record rec;
	if (length <= sizeof(rec)) {
		/* the region is not worth adding */
		return 0;
	}
	if (length + offset > tdb->file->map_size) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					"tdb1_free_region: adding region beyond"
					" end of file");
		return -1;
	}
	memset(&rec,'\0',sizeof(rec));
	rec.rec_len = length - sizeof(rec);
	if (tdb1_free(tdb, offset, &rec) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_free_region: failed to add free record");
		return -1;
	}
	return 0;
}

/*
  wipe the entire database, deleting all records. This can be done
  very fast by using a allrecord lock. The entire data portion of the
  file becomes a single entry in the freelist.

  This code carefully steps around the recovery area, leaving it alone
 */
int tdb1_wipe_all(struct tdb1_context *tdb)
{
	int i;
	tdb1_off_t offset = 0;
	ssize_t data_len;
	tdb1_off_t recovery_head;
	tdb1_len_t recovery_size = 0;

	if (tdb1_lockall(tdb) != 0) {
		return -1;
	}


	/* see if the tdb has a recovery area, and remember its size
	   if so. We don't want to lose this as otherwise each
	   tdb1_wipe_all() in a transaction will increase the size of
	   the tdb by the size of the recovery area */
	if (tdb1_ofs_read(tdb, TDB1_RECOVERY_HEAD, &recovery_head) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_wipe_all: failed to read recovery head");
		goto failed;
	}

	if (recovery_head != 0) {
		struct tdb1_record rec;
		if (tdb->methods->tdb1_read(tdb, recovery_head, &rec, sizeof(rec), TDB1_DOCONV()) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_wipe_all: failed to read recovery record");
			return -1;
		}
		recovery_size = rec.rec_len + sizeof(rec);
	}

	/* wipe the hashes */
	for (i=0;i<tdb->header.hash_size;i++) {
		if (tdb1_ofs_write(tdb, TDB1_HASH_TOP(i), &offset) == -1) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_wipe_all: failed to write hash %d", i);
			goto failed;
		}
	}

	/* wipe the freelist */
	if (tdb1_ofs_write(tdb, TDB1_FREELIST_TOP, &offset) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_wipe_all: failed to write freelist");
		goto failed;
	}

	/* add all the rest of the file to the freelist, possibly leaving a gap
	   for the recovery area */
	if (recovery_size == 0) {
		/* the simple case - the whole file can be used as a freelist */
		data_len = (tdb->file->map_size - TDB1_DATA_START(tdb->header.hash_size));
		if (tdb1_free_region(tdb, TDB1_DATA_START(tdb->header.hash_size), data_len) != 0) {
			goto failed;
		}
	} else {
		/* we need to add two freelist entries - one on either
		   side of the recovery area

		   Note that we cannot shift the recovery area during
		   this operation. Only the transaction.c code may
		   move the recovery area or we risk subtle data
		   corruption
		*/
		data_len = (recovery_head - TDB1_DATA_START(tdb->header.hash_size));
		if (tdb1_free_region(tdb, TDB1_DATA_START(tdb->header.hash_size), data_len) != 0) {
			goto failed;
		}
		/* and the 2nd free list entry after the recovery area - if any */
		data_len = tdb->file->map_size - (recovery_head+recovery_size);
		if (tdb1_free_region(tdb, recovery_head+recovery_size, data_len) != 0) {
			goto failed;
		}
	}

	if (tdb1_unlockall(tdb) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_wipe_all: failed to unlock");
		goto failed;
	}

	return 0;

failed:
	tdb1_unlockall(tdb);
	return -1;
}

struct traverse_state {
	enum TDB_ERROR error;
	struct tdb1_context *dest_db;
};

/*
  traverse function for repacking
 */
static int repack_traverse(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA data, void *private_data)
{
	struct traverse_state *state = (struct traverse_state *)private_data;
	if (tdb1_store(state->dest_db, key, data, TDB_INSERT) != 0) {
		state->error = state->dest_db->last_error;
		return -1;
	}
	return 0;
}

/*
  repack a tdb
 */
int tdb1_repack(struct tdb1_context *tdb)
{
	struct tdb1_context *tmp_db;
	struct traverse_state state;

	if (tdb1_transaction_start(tdb) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   __location__ " Failed to start transaction");
		return -1;
	}

	tmp_db = tdb1_open("tmpdb", tdb1_hash_size(tdb), TDB_INTERNAL, O_RDWR|O_CREAT, 0);
	if (tmp_db == NULL) {
		tdb->last_error = tdb_logerr(tdb, TDB_ERR_OOM, TDB_LOG_ERROR,
					__location__ " Failed to create tmp_db");
		tdb1_transaction_cancel(tdb);
		return -1;
	}

	state.error = TDB_SUCCESS;
	state.dest_db = tmp_db;

	if (tdb1_traverse_read(tdb, repack_traverse, &state) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   __location__ " Failed to traverse copying out");
		tdb1_transaction_cancel(tdb);
		tdb1_close(tmp_db);
		return -1;
	}

	if (state.error != TDB_SUCCESS) {
		tdb->last_error = tdb_logerr(tdb, state.error, TDB_LOG_ERROR,
					__location__ " Error during traversal");
		tdb1_transaction_cancel(tdb);
		tdb1_close(tmp_db);
		return -1;
	}

	if (tdb1_wipe_all(tdb) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   __location__ " Failed to wipe database\n");
		tdb1_transaction_cancel(tdb);
		tdb1_close(tmp_db);
		return -1;
	}

	state.error = TDB_SUCCESS;
	state.dest_db = tdb;

	if (tdb1_traverse_read(tmp_db, repack_traverse, &state) == -1) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   __location__ " Failed to traverse copying back");
		tdb1_transaction_cancel(tdb);
		tdb1_close(tmp_db);
		return -1;
	}

	if (state.error) {
		tdb->last_error = tdb_logerr(tdb, state.error, TDB_LOG_ERROR,
					__location__ " Error during second traversal");
		tdb1_transaction_cancel(tdb);
		tdb1_close(tmp_db);
		return -1;
	}

	tdb1_close(tmp_db);

	if (tdb1_transaction_commit(tdb) != 0) {
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   __location__ " Failed to commit");
		return -1;
	}

	return 0;
}

/* Even on files, we can get partial writes due to signals. */
bool tdb1_write_all(int fd, const void *buf, size_t count)
{
	while (count) {
		ssize_t ret;
		ret = write(fd, buf, count);
		if (ret < 0)
			return false;
		buf = (const char *)buf + ret;
		count -= ret;
	}
	return true;
}
