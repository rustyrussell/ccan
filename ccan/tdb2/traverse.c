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
	uint64_t i, num, count = 0;
	tdb_off_t off, prev_bucket;
	struct tdb_used_record rec;
	struct tdb_data k, d;
	bool finish = false;

	/* FIXME: Do we need to start at 0? */
	prev_bucket = tdb_lock_list(tdb, 0, ltype, TDB_LOCK_WAIT);
	if (prev_bucket != 0)
		return -1;

	num = (1ULL << tdb->header.v.hash_bits);

	for (i = tdb_find_nonzero_off(tdb, hash_off(tdb, 0), num);
	     i != num && !finish;
	     i += tdb_find_nonzero_off(tdb, hash_off(tdb, i), num - i)) {
		if (tdb_lock_list(tdb, i, ltype, TDB_LOCK_WAIT) != i)
			goto fail;

		off = tdb_read_off(tdb, hash_off(tdb, i));
		if (off == TDB_OFF_ERR) {
			tdb_unlock_list(tdb, i, ltype);
			goto fail;
		}

		/* This race can happen, but look again. */
		if (off == 0) {
			tdb_unlock_list(tdb, i, ltype);
			continue;
		}

		/* Drop previous lock. */
		tdb_unlock_list(tdb, prev_bucket, ltype);
		prev_bucket = i;

		if (tdb_read_convert(tdb, off, &rec, sizeof(rec)) != 0)
			goto fail;

		k.dsize = rec_key_length(&rec);
		d.dsize = rec_data_length(&rec);
		if (ltype == F_RDLCK) {
			/* Read traverses can keep the lock. */
			k.dptr = (void *)tdb_access_read(tdb,
							 off + sizeof(rec),
							 k.dsize + d.dsize,
							 false);
		} else {
			k.dptr = tdb_alloc_read(tdb, off + sizeof(rec),
						k.dsize + d.dsize);
		}
		if (!k.dptr)
			goto fail;
		d.dptr = k.dptr + k.dsize;
		count++;

		if (ltype == F_WRLCK) {
			/* Drop lock before calling out. */
			tdb_unlock_list(tdb, i, ltype);
		}

		if (fn && fn(tdb, k, d, p))
			finish = true;

		if (ltype == F_WRLCK) {
			free(k.dptr);
			/* Regain lock.  FIXME: Is this necessary? */
			if (tdb_lock_list(tdb, i, ltype, TDB_LOCK_WAIT) != i)
				return -1;

			/* This makes deleting under ourselves a bit nicer. */
			if (tdb_read_off(tdb, hash_off(tdb, i)) == off)
				i++;
		} else {
			tdb_access_release(tdb, k.dptr);
			i++;
		}
	}

	/* Drop final lock. */
	tdb_unlock_list(tdb, prev_bucket, ltype);
	return count;

fail:
	tdb_unlock_list(tdb, prev_bucket, ltype);
	return -1;
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
