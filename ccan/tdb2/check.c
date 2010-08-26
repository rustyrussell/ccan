 /* 
   Trivial Database 2: free list/block handling
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
#include <ccan/asearch/asearch.h>

/* We keep an ordered array of offsets. */
static bool append(tdb_off_t **arr, size_t *num, tdb_off_t off)
{
	tdb_off_t *new = realloc(*arr, (*num + 1) * sizeof(tdb_off_t));
	if (!new)
		return false;
	new[(*num)++] = off;
	*arr = new;
	return true;
}

static bool check_header(struct tdb_context *tdb)
{
	uint64_t hash_test;

	hash_test = TDB_HASH_MAGIC;
	hash_test = tdb_hash(tdb, &hash_test, sizeof(hash_test));
	if (tdb->header.hash_test != hash_test) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: hash test %llu should be %llu\n",
			 tdb->header.hash_test, hash_test);
		return false;
	}
	if (strcmp(tdb->header.magic_food, TDB_MAGIC_FOOD) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: bad magic '%.*s'\n",
			 sizeof(tdb->header.magic_food),
			 tdb->header.magic_food);
		return false;
	}
	if (tdb->header.v.hash_bits < INITIAL_HASH_BITS) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: bad hash bits %llu\n",
			 (long long)tdb->header.v.hash_bits);
		return false;
	}
	if (tdb->header.v.zone_bits < INITIAL_ZONE_BITS) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: bad zone_bits %llu\n",
			 (long long)tdb->header.v.zone_bits);
		return false;
	}
	if (tdb->header.v.free_buckets < INITIAL_FREE_BUCKETS) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: bad free_buckets %llu\n",
			 (long long)tdb->header.v.free_buckets);
		return false;
	}
	if ((1ULL << tdb->header.v.zone_bits) * tdb->header.v.num_zones
	    < tdb->map_size) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: %llu zones size %llu don't cover %llu\n",
			 (long long)tdb->header.v.num_zones,
			 (long long)(1ULL << tdb->header.v.zone_bits),
			 (long long)tdb->map_size);
		return false;
	}

	/* We check hash_off and free_off later. */

	/* Don't check reserved: they *can* be used later. */
	return true;
}

static int off_cmp(const tdb_off_t *a, const tdb_off_t *b)
{
	/* Can overflow an int. */
	return *a > *b ? 1
		: *a < *b ? -1
		: 0;
}

static bool check_hash_list(struct tdb_context *tdb,
			    tdb_off_t used[],
			    size_t num_used)
{
	struct tdb_used_record rec;
	tdb_len_t hashlen, i, num_nonzero;
	tdb_off_t h;
	size_t num_found;

	hashlen = sizeof(tdb_off_t) << tdb->header.v.hash_bits;

	if (tdb_read_convert(tdb, tdb->header.v.hash_off - sizeof(rec),
			     &rec, sizeof(rec)) == -1)
		return false;

	if (rec_data_length(&rec) != hashlen) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad hash table length %llu vs %llu\n",
			 (long long)rec_data_length(&rec),
			 (long long)hashlen);
		return false;
	}
	if (rec_key_length(&rec) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad hash table key length %llu\n",
			 (long long)rec_key_length(&rec));
		return false;
	}
	if (rec_hash(&rec) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad hash table hash value %llu\n",
			 (long long)rec_hash(&rec));
		return false;
	}

	num_found = 0;
	num_nonzero = 0;
	for (i = 0, h = tdb->header.v.hash_off;
	     i < (1ULL << tdb->header.v.hash_bits);
	     i++, h += sizeof(tdb_off_t)) {
		tdb_off_t off, *p, pos;
		struct tdb_used_record rec;
		uint64_t hash;

		off = tdb_read_off(tdb, h);
		if (off == TDB_OFF_ERR)
			return false;
		if (!off) {
			num_nonzero = 0;
			continue;
		}
		/* FIXME: Check hash bits */
		p = asearch(&off, used, num_used, off_cmp);
		if (!p) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_check: Invalid offset %llu in hash\n",
				 (long long)off);
			return false;
		}
		/* Mark it invalid. */
		*p ^= 1;
		num_found++;

		if (tdb_read_convert(tdb, off, &rec, sizeof(rec)) == -1)
			return false;

		/* Check it is hashed correctly. */
		hash = hash_record(tdb, off);

		/* Top bits must match header. */
		if (hash >> (64 - 11) != rec_hash(&rec)) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_check: Bad hash magic at offset %llu"
				 " (0x%llx vs 0x%llx)\n",
				 (long long)off,
				 (long long)hash, (long long)rec_hash(&rec));
			return false;
		}

		/* It must be in the right place in hash array. */
		pos = hash & ((1ULL << tdb->header.v.hash_bits)-1);
		if (pos < i - num_nonzero || pos > i) {
			/* Could be wrap from end of array?  FIXME: check? */
			if (i != num_nonzero) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: Bad hash position %llu at"
					 " offset %llu hash 0x%llx\n",
					 (long long)i,
					 (long long)off,
					 (long long)hash);
				return false;
			}
		}
		num_nonzero++;
	}

	/* free table and hash table are two of the used blocks. */
	if (num_found != num_used - 2) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Not all entries are in hash\n");
		return false;
	}
	return true;
}

static bool check_free(struct tdb_context *tdb,
		       tdb_off_t off,
		       const struct tdb_free_record *frec,
		       tdb_off_t prev,
		       tdb_off_t zone, unsigned int bucket)
{
	if (frec->magic != TDB_FREE_MAGIC) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu bad magic 0x%llx\n",
			 (long long)off, (long long)frec->magic);
		return false;
	}
	if (tdb->methods->oob(tdb, off
			      + frec->data_len-sizeof(struct tdb_used_record),
			      true))
		return false;
	if (zone_of(tdb, off) != zone) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu in wrong zone %llu vs %llu\n",
			 (long long)off,
			 (long long)zone, (long long)zone_of(tdb, off));
		return false;
	}
	if (size_to_bucket(tdb, frec->data_len) != bucket) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu in wrong bucket %u vs %u\n",
			 (long long)off,
			 bucket, size_to_bucket(tdb, frec->data_len));
		return false;
	}
	if (prev != frec->prev) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu bad prev %llu vs %llu\n",
			 (long long)off,
			 (long long)prev, (long long)frec->prev);
		return false;
	}
	return true;
}
		       
static bool check_free_list(struct tdb_context *tdb,
			    tdb_off_t free[],
			    size_t num_free)
{
	struct tdb_used_record rec;
	tdb_len_t freelen, i, j;
	tdb_off_t h;
	size_t num_found;

	freelen = sizeof(tdb_off_t) * tdb->header.v.num_zones
		* (tdb->header.v.free_buckets + 1);

	if (tdb_read_convert(tdb, tdb->header.v.free_off - sizeof(rec),
			     &rec, sizeof(rec)) == -1)
		return false;

	if (rec_data_length(&rec) != freelen) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad free table length %llu vs %llu\n",
			 (long long)rec_data_length(&rec),
			 (long long)freelen);
		return false;
	}
	if (rec_key_length(&rec) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad free table key length %llu\n",
			 (long long)rec_key_length(&rec));
		return false;
	}
	if (rec_hash(&rec) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad free table hash value %llu\n",
			 (long long)rec_hash(&rec));
		return false;
	}

	num_found = 0;
	h = tdb->header.v.free_off;
	for (i = 0; i < tdb->header.v.num_zones; i++) {
		for (j = 0; j <= tdb->header.v.free_buckets;
		     j++, h += sizeof(tdb_off_t)) {
			tdb_off_t off, prev = 0, *p;
			struct tdb_free_record f;

			for (off = tdb_read_off(tdb, h); off; off = f.next) {
				if (off == TDB_OFF_ERR)
					return false;
				if (tdb_read_convert(tdb, off, &f, sizeof(f)))
					return false;
				if (!check_free(tdb, off, &f, prev, i, j))
					return false;

				/* FIXME: Check hash bits */
				p = asearch(&off, free, num_free, off_cmp);
				if (!p) {
					tdb->log(tdb, TDB_DEBUG_ERROR,
						 tdb->log_priv,
						 "tdb_check: Invalid offset"
						 " %llu in free table\n",
						 (long long)off);
					return false;
				}
				/* Mark it invalid. */
				*p ^= 1;
				num_found++;
				prev = off;
			}
		}
	}
	if (num_found != num_free) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Not all entries are in free table\n");
		return false;
	}
	return true;
}

/* FIXME: call check() function. */
int tdb_check(struct tdb_context *tdb,
	      int (*check)(TDB_DATA key, TDB_DATA data, void *private_data),
	      void *private_data)
{
	tdb_off_t *free = NULL, *used = NULL, off;
	tdb_len_t len;
	size_t num_free = 0, num_used = 0;
	bool hash_found = false, free_found = false;

	/* This always ensures the header is uptodate. */
	if (tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false) != 0)
		return -1;

	if (!check_header(tdb))
		goto fail;

	/* First we do a linear scan, checking all records. */
	for (off = sizeof(struct tdb_header);
	     off < tdb->map_size;
	     off += len) {
		union {
			struct tdb_used_record u;
			struct tdb_free_record f;
		} pad, *p;
		p = tdb_get(tdb, off, &pad, sizeof(pad));
		if (!p)
			goto fail;
		if (p->f.magic == TDB_FREE_MAGIC) {
			/* This record is free! */
			if (!append(&free, &num_free, off))
				goto fail;
			len = sizeof(p->u) + p->f.data_len;
			if (tdb->methods->oob(tdb, off + len, false))
				goto fail;
		} else {
			uint64_t klen, dlen, extra;

			/* This record is used! */
			if (rec_magic(&p->u) != TDB_MAGIC) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: Bad magic 0x%llx"
					 " at offset %llu\n",
					 (long long)rec_magic(&p->u),
					 (long long)off);
				goto fail;
			}
			
			if (!append(&used, &num_used, off))
				goto fail;

			klen = rec_key_length(&p->u);
			dlen = rec_data_length(&p->u);
			extra = rec_extra_padding(&p->u);

			len = sizeof(p->u) + klen + dlen + extra;
			if (tdb->methods->oob(tdb, off + len, false))
				goto fail;

			if (off + sizeof(p->u) == tdb->header.v.hash_off) {
				hash_found = true;
			} else if (off + sizeof(p->u)
				   == tdb->header.v.free_off) {
				free_found = true;
			}
		}
	}

	if (!hash_found) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: hash table not found at %llu\n",
			 (long long)tdb->header.v.hash_off);
		goto fail;
	}

	if (!free_found) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: free table not found at %llu\n",
			 (long long)tdb->header.v.free_off);
		goto fail;
	}

	/* FIXME: Check key uniqueness? */
	if (!check_hash_list(tdb, used, num_used))
		goto fail;

	if (!check_free_list(tdb, free, num_free))
		goto fail;

	tdb_allrecord_unlock(tdb, F_RDLCK);
	return 0;

fail:
	tdb_allrecord_unlock(tdb, F_RDLCK);
	return -1;
}
