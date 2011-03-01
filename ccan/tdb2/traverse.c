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

int64_t tdb_traverse(struct tdb_context *tdb, tdb_traverse_func fn, void *p)
{
	enum TDB_ERROR ecode;
	struct traverse_info tinfo;
	struct tdb_data k, d;
	int64_t count = 0;

	k.dptr = NULL;
	for (ecode = first_in_hash(tdb, &tinfo, &k, &d.dsize);
	     ecode == TDB_SUCCESS;
	     ecode = next_in_hash(tdb, &tinfo, &k, &d.dsize)) {
		d.dptr = k.dptr + k.dsize;
		
		count++;
		if (fn && fn(tdb, k, d, p)) {
			free(k.dptr);
			break;
		}
		free(k.dptr);
	}

	if (ecode != TDB_ERR_NOEXIST) {
		tdb->ecode = ecode;
		return -1;
	}
	return count;
}
	
TDB_DATA tdb_firstkey(struct tdb_context *tdb)
{
	struct traverse_info tinfo;
	struct tdb_data k;
	enum TDB_ERROR ecode;

	ecode = first_in_hash(tdb, &tinfo, &k, NULL);
	if (ecode == TDB_SUCCESS) {
		return k;
	}
	if (ecode == TDB_ERR_NOEXIST)
		ecode = TDB_SUCCESS;
	tdb->ecode = ecode;
	return tdb_null;
}

/* We lock twice, not very efficient.  We could keep last key & tinfo cached. */
TDB_DATA tdb_nextkey(struct tdb_context *tdb, TDB_DATA key)
{
	struct traverse_info tinfo;
	struct hash_info h;
	struct tdb_used_record rec;
	enum TDB_ERROR ecode;

	tinfo.prev = find_and_lock(tdb, key, F_RDLCK, &h, &rec, &tinfo);
	if (TDB_OFF_IS_ERR(tinfo.prev)) {
		tdb->ecode = tinfo.prev;
		return tdb_null;
	}
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

	ecode = next_in_hash(tdb, &tinfo, &key, NULL);
	if (ecode == TDB_SUCCESS) {
		return key;
	}
	if (ecode == TDB_ERR_NOEXIST)
		ecode = TDB_SUCCESS;
	tdb->ecode = ecode;
	return tdb_null;
}
