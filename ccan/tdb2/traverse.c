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

static int64_t traverse(struct tdb_context *tdb, int ltype,
			tdb_traverse_func fn, void *p)
{
	int ret;
	struct traverse_info tinfo;
	struct tdb_data k, d;
	int64_t count = 0;

	k.dptr = NULL;
	for (ret = first_in_hash(tdb, ltype, &tinfo, &k, &d.dsize);
	     ret == 1;
	     ret = next_in_hash(tdb, ltype, &tinfo, &k, &d.dsize)) {
		d.dptr = k.dptr + k.dsize;
		
		count++;
		if (fn && fn(tdb, k, d, p)) {
			free(k.dptr);
			break;
		}
		free(k.dptr);
	}

	if (ret < 0)
		return -1;
	return count;
}

int64_t tdb_traverse(struct tdb_context *tdb, tdb_traverse_func fn, void *p)
{
	return traverse(tdb, F_WRLCK, fn, p);
}
	
int64_t tdb_traverse_read(struct tdb_context *tdb,
			  tdb_traverse_func fn, void *p)
{
	int64_t ret;
	bool was_ro = tdb->read_only;
	tdb->read_only = true;
	ret = traverse(tdb, F_RDLCK, fn, p);
	tdb->read_only = was_ro;
	return ret;
}

TDB_DATA tdb_firstkey(struct tdb_context *tdb)
{
	struct traverse_info tinfo;
	struct tdb_data k;
	switch (first_in_hash(tdb, F_RDLCK, &tinfo, &k, NULL)) {
	case 1:
		return k;
	case 0:
		tdb->ecode = TDB_SUCCESS;
		/* Fall thru... */
	default:
		return tdb_null;
	}
}

/* We lock twice, not very efficient.  We could keep last key & tinfo cached. */
TDB_DATA tdb_nextkey(struct tdb_context *tdb, TDB_DATA key)
{
	struct traverse_info tinfo;
	struct hash_info h;
	struct tdb_used_record rec;

	tinfo.prev = find_and_lock(tdb, key, F_RDLCK, &h, &rec, &tinfo);
	if (unlikely(tinfo.prev == TDB_OFF_ERR))
		return tdb_null;
	tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

	switch (next_in_hash(tdb, F_RDLCK, &tinfo, &key, NULL)) {
	case 1:
		return key;
	case 0:
		tdb->ecode = TDB_SUCCESS;
		/* Fall thru... */
	default:
		return tdb_null;
	}
}
