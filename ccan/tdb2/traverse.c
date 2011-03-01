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
			return count;
		}
		free(k.dptr);
	}

	if (ecode != TDB_ERR_NOEXIST) {
		return ecode;
	}
	return count;
}
	
enum TDB_ERROR tdb_firstkey(struct tdb_context *tdb, struct tdb_data *key)
{
	struct traverse_info tinfo;

	return first_in_hash(tdb, &tinfo, key, NULL);
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
		return tinfo.prev;
	}
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

	return next_in_hash(tdb, &tinfo, key, NULL);
}
