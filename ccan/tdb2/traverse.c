 /*
   Trivial Database 2: traverse function.
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
#include "private.h"
#include <ccan/likely/likely.h>

int64_t tdb_traverse_(struct tdb_context *tdb,
		      int (*fn)(struct tdb_context *,
				TDB_DATA, TDB_DATA, void *),
		      void *p)
{
	enum TDB_ERROR ecode;
	struct traverse_info tinfo;
	struct tdb_data k, d;
	int64_t count = 0;

	if (tdb->flags & TDB_VERSION1) {
		count = tdb1_traverse(tdb, fn, p);
		if (count == -1)
			return tdb->last_error;
		return count;
	}

	k.dptr = NULL;
	for (ecode = first_in_hash(tdb, &tinfo, &k, &d.dsize);
	     ecode == TDB_SUCCESS;
	     ecode = next_in_hash(tdb, &tinfo, &k, &d.dsize)) {
		d.dptr = k.dptr + k.dsize;
		
		count++;
		if (fn && fn(tdb, k, d, p)) {
			free(k.dptr);
			tdb->last_error = TDB_SUCCESS;
			return count;
		}
		free(k.dptr);
	}

	if (ecode != TDB_ERR_NOEXIST) {
		return tdb->last_error = ecode;
	}
	tdb->last_error = TDB_SUCCESS;
	return count;
}
	
enum TDB_ERROR tdb_firstkey(struct tdb_context *tdb, struct tdb_data *key)
{
	struct traverse_info tinfo;

	return tdb->last_error = first_in_hash(tdb, &tinfo, key, NULL);
}

/* We lock twice, not very efficient.  We could keep last key & tinfo cached. */
enum TDB_ERROR tdb_nextkey(struct tdb_context *tdb, struct tdb_data *key)
{
	struct traverse_info tinfo;
	struct hash_info h;
	struct tdb_used_record rec;

	tinfo.prev = find_and_lock(tdb, *key, F_RDLCK, &h, &rec, &tinfo);
	free(key->dptr);
	if (TDB_OFF_IS_ERR(tinfo.prev)) {
		return tdb->last_error = tinfo.prev;
	}
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

	return tdb->last_error = next_in_hash(tdb, &tinfo, key, NULL);
}

static int wipe_one(struct tdb_context *tdb,
		    TDB_DATA key, TDB_DATA data, enum TDB_ERROR *ecode)
{
	*ecode = tdb_delete(tdb, key);
	return (*ecode != TDB_SUCCESS);
}

enum TDB_ERROR tdb_wipe_all(struct tdb_context *tdb)
{
	enum TDB_ERROR ecode;
	int64_t count;

	ecode = tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false);
	if (ecode != TDB_SUCCESS)
		return tdb->last_error = ecode;

	/* FIXME: Be smarter. */
	count = tdb_traverse(tdb, wipe_one, &ecode);
	if (count < 0)
		ecode = count;
	tdb_allrecord_unlock(tdb, F_WRLCK);
	return tdb->last_error = ecode;
}
