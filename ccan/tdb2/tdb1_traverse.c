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

#define TDB1_NEXT_LOCK_ERR ((tdb1_off_t)-1)

/* Uses traverse lock: 0 = finish, TDB1_NEXT_LOCK_ERR = error,
   other = record offset */
static tdb1_off_t tdb1_next_lock(struct tdb1_context *tdb, struct tdb1_traverse_lock *tlock,
			 struct tdb1_record *rec)
{
	int want_next = (tlock->off != 0);

	/* Lock each chain from the start one. */
	for (; tlock->hash < tdb->header.hash_size; tlock->hash++) {
		if (!tlock->off && tlock->hash != 0) {
			/* this is an optimisation for the common case where
			   the hash chain is empty, which is particularly
			   common for the use of tdb with ldb, where large
			   hashes are used. In that case we spend most of our
			   time in tdb1_brlock(), locking empty hash chains.

			   To avoid this, we do an unlocked pre-check to see
			   if the hash chain is empty before starting to look
			   inside it. If it is empty then we can avoid that
			   hash chain. If it isn't empty then we can't believe
			   the value we get back, as we read it without a
			   lock, so instead we get the lock and re-fetch the
			   value below.

			   Notice that not doing this optimisation on the
			   first hash chain is critical. We must guarantee
			   that we have done at least one fcntl lock at the
			   start of a search to guarantee that memory is
			   coherent on SMP systems. If records are added by
			   others during the search then thats OK, and we
			   could possibly miss those with this trick, but we
			   could miss them anyway without this trick, so the
			   semantics don't change.

			   With a non-indexed ldb search this trick gains us a
			   factor of around 80 in speed on a linux 2.6.x
			   system (testing using ldbtest).
			*/
			tdb->methods->next_hash_chain(tdb, &tlock->hash);
			if (tlock->hash == tdb->header.hash_size) {
				continue;
			}
		}

		if (tdb1_lock(tdb, tlock->hash, tlock->lock_rw) == -1)
			return TDB1_NEXT_LOCK_ERR;

		/* No previous record?  Start at top of chain. */
		if (!tlock->off) {
			if (tdb1_ofs_read(tdb, TDB1_HASH_TOP(tlock->hash),
				     &tlock->off) == -1)
				goto fail;
		} else {
			/* Otherwise unlock the previous record. */
			if (tdb1_unlock_record(tdb, tlock->off) != 0)
				goto fail;
		}

		if (want_next) {
			/* We have offset of old record: grab next */
			if (tdb1_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;
			tlock->off = rec->next;
		}

		/* Iterate through chain */
		while( tlock->off) {
			tdb1_off_t current;
			if (tdb1_rec_read(tdb, tlock->off, rec) == -1)
				goto fail;

			/* Detect infinite loops. From "Shlomi Yaakobovich" <Shlomi@exanet.com>. */
			if (tlock->off == rec->next) {
				tdb->last_error = tdb_logerr(tdb, TDB_ERR_CORRUPT,
							TDB_LOG_ERROR,
							"tdb1_next_lock:"
							" loop detected.");
				goto fail;
			}

			if (!TDB1_DEAD(rec)) {
				/* Woohoo: we found one! */
				if (tdb1_lock_record(tdb, tlock->off) != 0)
					goto fail;
				return tlock->off;
			}

			/* Try to clean dead ones from old traverses */
			current = tlock->off;
			tlock->off = rec->next;
			if (!(tdb->read_only || tdb->traverse_read) &&
			    tdb1_do_delete(tdb, current, rec) != 0)
				goto fail;
		}
		tdb1_unlock(tdb, tlock->hash, tlock->lock_rw);
		want_next = 0;
	}
	/* We finished iteration without finding anything */
	tdb->last_error = TDB_SUCCESS;
	return 0;

 fail:
	tlock->off = 0;
	if (tdb1_unlock(tdb, tlock->hash, tlock->lock_rw) != 0)
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_next_lock: On error unlock failed!");
	return TDB1_NEXT_LOCK_ERR;
}

/* traverse the entire database - calling fn(tdb, key, data) on each element.
   return -1 on error or the record count traversed
   if fn is NULL then it is not called
   a non-zero return value from fn() indicates that the traversal should stop
  */
static int tdb1_traverse_internal(struct tdb1_context *tdb,
				 tdb1_traverse_func fn, void *private_data,
				 struct tdb1_traverse_lock *tl)
{
	TDB_DATA key, dbuf;
	struct tdb1_record rec;
	int ret = 0, count = 0;
	tdb1_off_t off;

	/* This was in the initializaton, above, but the IRIX compiler
	 * did not like it.  crh
	 */
	tl->next = tdb->travlocks.next;

	/* fcntl locks don't stack: beware traverse inside traverse */
	tdb->travlocks.next = tl;

	/* tdb1_next_lock places locks on the record returned, and its chain */
	while ((off = tdb1_next_lock(tdb, tl, &rec)) != 0) {
		if (off == TDB1_NEXT_LOCK_ERR) {
			ret = -1;
			goto out;
		}
		count++;
		/* now read the full record */
		key.dptr = tdb1_alloc_read(tdb, tl->off + sizeof(rec),
					  rec.key_len + rec.data_len);
		if (!key.dptr) {
			ret = -1;
			if (tdb1_unlock(tdb, tl->hash, tl->lock_rw) != 0)
				goto out;
			if (tdb1_unlock_record(tdb, tl->off) != 0)
				tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
					   "tdb1_traverse: key.dptr == NULL and"
					   " unlock_record failed!");
			goto out;
		}
		key.dsize = rec.key_len;
		dbuf.dptr = key.dptr + rec.key_len;
		dbuf.dsize = rec.data_len;

		/* Drop chain lock, call out */
		if (tdb1_unlock(tdb, tl->hash, tl->lock_rw) != 0) {
			ret = -1;
			SAFE_FREE(key.dptr);
			goto out;
		}
		if (fn && fn(tdb, key, dbuf, private_data)) {
			/* They want us to terminate traversal */
			if (tdb1_unlock_record(tdb, tl->off) != 0) {
				tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
					   "tdb1_traverse:"
					   " unlock_record failed!");
				ret = -1;
			}
			SAFE_FREE(key.dptr);
			goto out;
		}
		SAFE_FREE(key.dptr);
	}
out:
	tdb->travlocks.next = tl->next;
	if (ret < 0)
		return -1;
	else
		return count;
}


/*
  a write style traverse - temporarily marks the db read only
*/
int tdb1_traverse_read(struct tdb1_context *tdb,
		      tdb1_traverse_func fn, void *private_data)
{
	struct tdb1_traverse_lock tl = { NULL, 0, 0, F_RDLCK };
	int ret;

	/* we need to get a read lock on the transaction lock here to
	   cope with the lock ordering semantics of solaris10 */
	if (tdb1_transaction_lock(tdb, F_RDLCK, TDB_LOCK_WAIT)) {
		return -1;
	}

	tdb->traverse_read++;
	ret = tdb1_traverse_internal(tdb, fn, private_data, &tl);
	tdb->traverse_read--;

	tdb1_transaction_unlock(tdb, F_RDLCK);

	return ret;
}

/*
  a write style traverse - needs to get the transaction lock to
  prevent deadlocks

  WARNING: The data buffer given to the callback fn does NOT meet the
  alignment restrictions malloc gives you.
*/
int tdb1_traverse(struct tdb1_context *tdb,
		 tdb1_traverse_func fn, void *private_data)
{
	struct tdb1_traverse_lock tl = { NULL, 0, 0, F_WRLCK };
	int ret;

	if (tdb->read_only || tdb->traverse_read) {
		return tdb1_traverse_read(tdb, fn, private_data);
	}

	if (tdb1_transaction_lock(tdb, F_WRLCK, TDB_LOCK_WAIT)) {
		return -1;
	}

	tdb->traverse_write++;
	ret = tdb1_traverse_internal(tdb, fn, private_data, &tl);
	tdb->traverse_write--;

	tdb1_transaction_unlock(tdb, F_WRLCK);

	return ret;
}


/* find the first entry in the database and return its key */
TDB_DATA tdb1_firstkey(struct tdb1_context *tdb)
{
	TDB_DATA key;
	struct tdb1_record rec;
	tdb1_off_t off;

	/* release any old lock */
	if (tdb1_unlock_record(tdb, tdb->travlocks.off) != 0)
		return tdb1_null;
	tdb->travlocks.off = tdb->travlocks.hash = 0;
	tdb->travlocks.lock_rw = F_RDLCK;

	/* Grab first record: locks chain and returned record. */
	off = tdb1_next_lock(tdb, &tdb->travlocks, &rec);
	if (off == 0 || off == TDB1_NEXT_LOCK_ERR) {
		return tdb1_null;
	}
	/* now read the key */
	key.dsize = rec.key_len;
	key.dptr =tdb1_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),key.dsize);

	/* Unlock the hash chain of the record we just read. */
	if (tdb1_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_firstkey:"
			   " error occurred while tdb1_unlocking!");
	return key;
}

/* find the next entry in the database, returning its key */
TDB_DATA tdb1_nextkey(struct tdb1_context *tdb, TDB_DATA oldkey)
{
	uint32_t oldhash;
	TDB_DATA key = tdb1_null;
	struct tdb1_record rec;
	unsigned char *k = NULL;
	tdb1_off_t off;

	/* Is locked key the old key?  If so, traverse will be reliable. */
	if (tdb->travlocks.off) {
		if (tdb1_lock(tdb,tdb->travlocks.hash,tdb->travlocks.lock_rw))
			return tdb1_null;
		if (tdb1_rec_read(tdb, tdb->travlocks.off, &rec) == -1
		    || !(k = tdb1_alloc_read(tdb,tdb->travlocks.off+sizeof(rec),
					    rec.key_len))
		    || memcmp(k, oldkey.dptr, oldkey.dsize) != 0) {
			/* No, it wasn't: unlock it and start from scratch */
			if (tdb1_unlock_record(tdb, tdb->travlocks.off) != 0) {
				SAFE_FREE(k);
				return tdb1_null;
			}
			if (tdb1_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0) {
				SAFE_FREE(k);
				return tdb1_null;
			}
			tdb->travlocks.off = 0;
		}

		SAFE_FREE(k);
	}

	if (!tdb->travlocks.off) {
		/* No previous element: do normal find, and lock record */
		tdb->travlocks.off = tdb1_find_lock_hash(tdb, oldkey, tdb_hash(tdb, oldkey.dptr, oldkey.dsize), tdb->travlocks.lock_rw, &rec);
		if (!tdb->travlocks.off) {
			return tdb1_null;
		}
		tdb->travlocks.hash = TDB1_BUCKET(rec.full_hash);
		if (tdb1_lock_record(tdb, tdb->travlocks.off) != 0) {
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_nextkey: lock_record failed (%s)!",
				   strerror(errno));
			return tdb1_null;
		}
	}
	oldhash = tdb->travlocks.hash;

	/* Grab next record: locks chain and returned record,
	   unlocks old record */
	off = tdb1_next_lock(tdb, &tdb->travlocks, &rec);
	if (off != TDB1_NEXT_LOCK_ERR && off != 0) {
		key.dsize = rec.key_len;
		key.dptr = tdb1_alloc_read(tdb, tdb->travlocks.off+sizeof(rec),
					  key.dsize);
		/* Unlock the chain of this new record */
		if (tdb1_unlock(tdb, tdb->travlocks.hash, tdb->travlocks.lock_rw) != 0)
			tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
				   "tdb1_nextkey: WARNING tdb1_unlock failed!");
	}
	/* Unlock the chain of old record */
	if (tdb1_unlock(tdb, TDB1_BUCKET(oldhash), tdb->travlocks.lock_rw) != 0)
		tdb_logerr(tdb, tdb->last_error, TDB_LOG_ERROR,
			   "tdb1_nextkey: WARNING tdb1_unlock failed!");
	return key;
}
