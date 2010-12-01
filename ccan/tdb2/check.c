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

static bool check_header(struct tdb_context *tdb, tdb_off_t *recovery)
{
	uint64_t hash_test;
	struct tdb_header hdr;

	if (tdb_read_convert(tdb, 0, &hdr, sizeof(hdr)) == -1)
		return false;
	/* magic food should not be converted, so convert back. */
	tdb_convert(tdb, hdr.magic_food, sizeof(hdr.magic_food));

	hash_test = TDB_HASH_MAGIC;
	hash_test = tdb_hash(tdb, &hash_test, sizeof(hash_test));
	if (hdr.hash_test != hash_test) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: hash test %llu should be %llu\n",
			 (long long)hdr.hash_test,
			 (long long)hash_test);
		return false;
	}

	if (strcmp(hdr.magic_food, TDB_MAGIC_FOOD) != 0) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "check: bad magic '%.*s'\n",
			 (unsigned)sizeof(hdr.magic_food), hdr.magic_food);
		return false;
	}

	*recovery = hdr.recovery;
	if (*recovery) {
		if (*recovery < sizeof(hdr) || *recovery > tdb->map_size) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "tdb_check: invalid recovery offset %zu\n",
				 (size_t)*recovery);
			return false;
		}
	}

	/* Don't check reserved: they *can* be used later. */
	return true;
}

static bool check_hash_tree(struct tdb_context *tdb,
			    tdb_off_t off, unsigned int group_bits,
			    uint64_t hprefix,
			    unsigned hprefix_bits,
			    tdb_off_t used[],
			    size_t num_used,
			    size_t *num_found,
			    int (*check)(TDB_DATA, TDB_DATA, void *),
			    void *private_data);

static bool check_hash_record(struct tdb_context *tdb,
			      tdb_off_t off,
			      uint64_t hprefix,
			      unsigned hprefix_bits,
			      tdb_off_t used[],
			      size_t num_used,
			      size_t *num_found,
			      int (*check)(TDB_DATA, TDB_DATA, void *),
			      void *private_data)
{
	struct tdb_used_record rec;

	if (tdb_read_convert(tdb, off, &rec, sizeof(rec)) == -1)
		return false;

	if (rec_data_length(&rec)
	    != sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Bad hash table length %llu vs %llu\n",
			 (long long)rec_data_length(&rec),
			 (long long)sizeof(tdb_off_t)<<TDB_SUBLEVEL_HASH_BITS);
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

	off += sizeof(rec);
	return check_hash_tree(tdb, off,
			       TDB_SUBLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS,
			       hprefix, hprefix_bits,
			       used, num_used, num_found, check, private_data);
}

static int off_cmp(const tdb_off_t *a, const tdb_off_t *b)
{
	/* Can overflow an int. */
	return *a > *b ? 1
		: *a < *b ? -1
		: 0;
}

static uint64_t get_bits(uint64_t h, unsigned num, unsigned *used)
{
	*used += num;

	return (h >> (64 - *used)) & ((1U << num) - 1);
}

static bool check_hash_tree(struct tdb_context *tdb,
			    tdb_off_t off, unsigned int group_bits,
			    uint64_t hprefix,
			    unsigned hprefix_bits,
			    tdb_off_t used[],
			    size_t num_used,
			    size_t *num_found,
			    int (*check)(TDB_DATA, TDB_DATA, void *),
			    void *private_data)
{
	unsigned int g, b;
	const tdb_off_t *hash;
	struct tdb_used_record rec;

	hash = tdb_access_read(tdb, off,
			       sizeof(tdb_off_t)
			       << (group_bits + TDB_HASH_GROUP_BITS),
			       true);
	if (!hash)
		return false;

	for (g = 0; g < (1 << group_bits); g++) {
		const tdb_off_t *group = hash + (g << TDB_HASH_GROUP_BITS);
		for (b = 0; b < (1 << TDB_HASH_GROUP_BITS); b++) {
			unsigned int bucket, i, used_bits;
			uint64_t h;
			tdb_off_t *p;
			if (group[b] == 0)
				continue;

			off = group[b] & TDB_OFF_MASK;
			p = asearch(&off, used, num_used, off_cmp);
			if (!p) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: Invalid offset %llu "
					 "in hash\n",
					 (long long)off);
				goto fail;
			}
			/* Mark it invalid. */
			*p ^= 1;
			(*num_found)++;

			if (is_subhash(group[b])) {
				uint64_t subprefix;
				subprefix = (hprefix 
				     << (group_bits + TDB_HASH_GROUP_BITS))
					+ g * (1 << TDB_HASH_GROUP_BITS) + b;

				if (!check_hash_record(tdb,
					       group[b] & TDB_OFF_MASK,
					       subprefix,
					       hprefix_bits
						       + group_bits
						       + TDB_HASH_GROUP_BITS,
					       used, num_used, num_found,
					       check, private_data))
					goto fail;
				continue;
			}
			/* A normal entry */

			/* Does it belong here at all? */
			h = hash_record(tdb, off);
			used_bits = 0;
			if (get_bits(h, hprefix_bits, &used_bits) != hprefix
			    && hprefix_bits) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "check: bad hash placement"
					 " 0x%llx vs 0x%llx\n",
					 (long long)h, (long long)hprefix);
				goto fail;
			}

			/* Does it belong in this group? */
			if (get_bits(h, group_bits, &used_bits) != g) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "check: bad group %llu vs %u\n",
					 (long long)h, g);
				goto fail;
			}

			/* Are bucket bits correct? */
			bucket = group[b] & TDB_OFF_HASH_GROUP_MASK;
			if (get_bits(h, TDB_HASH_GROUP_BITS, &used_bits)
			    != bucket) {
				used_bits -= TDB_HASH_GROUP_BITS;
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "check: bad bucket %u vs %u\n",
					 (unsigned)get_bits(h,
							    TDB_HASH_GROUP_BITS,
							    &used_bits),
					 bucket);
				goto fail;
			}

			/* There must not be any zero entries between
			 * the bucket it belongs in and this one! */
			for (i = bucket;
			     i != b;
			     i = (i + 1) % (1 << TDB_HASH_GROUP_BITS)) {
				if (group[i] == 0) {
					tdb->log(tdb, TDB_DEBUG_ERROR,
						 tdb->log_priv,
						 "check: bad group placement"
						 " %u vs %u\n",
						 b, bucket);
					goto fail;
				}
			}

			if (tdb_read_convert(tdb, off, &rec, sizeof(rec)) == -1)
				goto fail;

			/* Bottom bits must match header. */
			if ((h & ((1 << 11)-1)) != rec_hash(&rec)) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: Bad hash magic at"
					 " offset %llu (0x%llx vs 0x%llx)\n",
					 (long long)off,
					 (long long)h,
					 (long long)rec_hash(&rec));
				goto fail;
			}

			if (check) {
				TDB_DATA key, data;
				key.dsize = rec_key_length(&rec);
				data.dsize = rec_data_length(&rec);
				key.dptr = (void *)tdb_access_read(tdb,
						   off + sizeof(rec),
						   key.dsize + data.dsize,
						   false);
				if (!key.dptr)
					goto fail;
				data.dptr = key.dptr + key.dsize;
				if (check(key, data, private_data) != 0)
					goto fail;
				tdb_access_release(tdb, key.dptr);
			}
		}
	}
	tdb_access_release(tdb, hash);
	return true;

fail:
	tdb_access_release(tdb, hash);
	return false;
}

static bool check_hash(struct tdb_context *tdb,
		       tdb_off_t used[],
		       size_t num_used, size_t num_flists,
		       int (*check)(TDB_DATA, TDB_DATA, void *),
		       void *private_data)
{
	/* Free lists also show up as used. */
	size_t num_found = num_flists;

	if (!check_hash_tree(tdb, offsetof(struct tdb_header, hashtable),
			     TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS,
			     0, 0, used, num_used, &num_found,
			     check, private_data))
		return false;

	if (num_found != num_used) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Not all entries are in hash\n");
		return false;
	}
	return true;
}

static bool check_free(struct tdb_context *tdb,
		       tdb_off_t off,
		       const struct tdb_free_record *frec,
		       tdb_off_t prev, unsigned int flist, unsigned int bucket)
{
	if (frec_magic(frec) != TDB_FREE_MAGIC) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu bad magic 0x%llx\n",
			 (long long)off, (long long)frec->magic_and_prev);
		return false;
	}
	if (frec_flist(frec) != flist) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu bad freelist %u\n",
			 (long long)off, frec_flist(frec));
		return false;
	}

	if (tdb->methods->oob(tdb, off
			      + frec_len(frec) + sizeof(struct tdb_used_record),
			      false))
		return false;
	if (size_to_bucket(frec_len(frec)) != bucket) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu in wrong bucket %u vs %u\n",
			 (long long)off,
			 bucket, size_to_bucket(frec_len(frec)));
		return false;
	}
	if (prev != frec_prev(frec)) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: offset %llu bad prev %llu vs %llu\n",
			 (long long)off,
			 (long long)prev, (long long)frec_len(frec));
		return false;
	}
	return true;
}
		       
static bool check_free_list(struct tdb_context *tdb,
			    tdb_off_t flist_off,
			    unsigned flist_num,
			    tdb_off_t free[],
			    size_t num_free,
			    size_t *num_found)
{
	struct tdb_freelist flist;
	tdb_off_t h;
	unsigned int i;

	if (tdb_read_convert(tdb, flist_off, &flist, sizeof(flist)) == -1)
		return false;

	if (rec_magic(&flist.hdr) != TDB_MAGIC
	    || rec_key_length(&flist.hdr) != 0
	    || rec_data_length(&flist.hdr) != sizeof(flist) - sizeof(flist.hdr)
	    || rec_hash(&flist.hdr) != 1) {
		tdb->log(tdb, TDB_DEBUG_ERROR,
			 tdb->log_priv,
			 "tdb_check: Invalid header on free list\n");
		return false;
	}

	for (i = 0; i < TDB_FREE_BUCKETS; i++) {
		tdb_off_t off, prev = 0, *p;
		struct tdb_free_record f;

		h = bucket_off(flist_off, i);
		for (off = tdb_read_off(tdb, h); off; off = f.next) {
			if (off == TDB_OFF_ERR)
				return false;
			if (tdb_read_convert(tdb, off, &f, sizeof(f)))
				return false;
			if (!check_free(tdb, off, &f, prev, flist_num, i))
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
			(*num_found)++;
			prev = off;
		}
	}
	return true;
}

/* Slow, but should be very rare. */
size_t dead_space(struct tdb_context *tdb, tdb_off_t off)
{
	size_t len;

	for (len = 0; off + len < tdb->map_size; len++) {
		char c;
		if (tdb->methods->read(tdb, off, &c, 1))
			return 0;
		if (c != 0 && c != 0x43)
			break;
	}
	return len;
}

static bool check_linear(struct tdb_context *tdb,
			 tdb_off_t **used, size_t *num_used,
			 tdb_off_t **free, size_t *num_free,
			 tdb_off_t recovery)
{
	tdb_off_t off;
	tdb_len_t len;
	bool found_recovery = false;

	for (off = sizeof(struct tdb_header); off < tdb->map_size; off += len) {
		union {
			struct tdb_used_record u;
			struct tdb_free_record f;
			struct tdb_recovery_record r;
		} pad, *p;
		/* r is larger: only get that if we need to. */
		p = tdb_get(tdb, off, &pad, sizeof(pad.f));
		if (!p)
			return false;

		/* If we crash after ftruncate, we can get zeroes or fill. */
		if (p->r.magic == TDB_RECOVERY_INVALID_MAGIC
		    || p->r.magic ==  0x4343434343434343ULL) {
			p = tdb_get(tdb, off, &pad, sizeof(pad.r));
			if (!p)
				return false;
			if (recovery == off) {
				found_recovery = true;
				len = sizeof(p->r) + p->r.max_len;
			} else {
				len = dead_space(tdb, off);
				if (len < sizeof(p->r)) {
					tdb->log(tdb, TDB_DEBUG_ERROR,
						 tdb->log_priv,
						 "tdb_check: invalid dead space"
						 " at %zu\n", (size_t)off);
					return false;
				}

				tdb->log(tdb, TDB_DEBUG_WARNING, tdb->log_priv,
					 "Dead space at %zu-%zu (of %zu)\n",
					 (size_t)off, (size_t)(off + len),
					 (size_t)tdb->map_size);
			}
		} else if (p->r.magic == TDB_RECOVERY_MAGIC) {
			p = tdb_get(tdb, off, &pad, sizeof(pad.r));
			if (!p)
				return false;
			if (recovery != off) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: unexpected recovery"
					 " record at offset %zu\n",
					 (size_t)off);
				return false;
			}
			if (p->r.len > p->r.max_len) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: invalid recovery length"
					 " %zu\n", (size_t)p->r.len);
				return false;
			}
			if (p->r.eof > tdb->map_size) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: invalid old EOF"
					 " %zu\n", (size_t)p->r.eof);
				return false;
			}
			found_recovery = true;
			len = sizeof(p->r) + p->r.max_len;
		} else if (frec_magic(&p->f) == TDB_FREE_MAGIC
			   || frec_magic(&p->f) == TDB_COALESCING_MAGIC) {
			len = sizeof(p->u) + frec_len(&p->f);
			if (off + len > tdb->map_size) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: free overlength %llu"
					 " at offset %llu\n",
					 (long long)len, (long long)off);
				return false;
			}
			/* This record is free! */
			if (frec_magic(&p->f) == TDB_FREE_MAGIC
			    && !append(free, num_free, off))
				return false;
		} else {
			uint64_t klen, dlen, extra;

			/* This record is used! */
			if (rec_magic(&p->u) != TDB_MAGIC) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: Bad magic 0x%llx"
					 " at offset %llu\n",
					 (long long)rec_magic(&p->u),
					 (long long)off);
				return false;
			}

			if (!append(used, num_used, off))
				return false;

			klen = rec_key_length(&p->u);
			dlen = rec_data_length(&p->u);
			extra = rec_extra_padding(&p->u);

			len = sizeof(p->u) + klen + dlen + extra;
			if (off + len > tdb->map_size) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: used overlength %llu"
					 " at offset %llu\n",
					 (long long)len, (long long)off);
				return false;
			}

			if (len < sizeof(p->f)) {
				tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
					 "tdb_check: too short record %llu at"
					 " %llu\n",
					 (long long)len, (long long)off);
				return false;
			}
		}
	}

	/* We must have found recovery area if there was one. */
	if (recovery != 0 && !found_recovery) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: expected a recovery area at %zu\n",
			 (size_t)recovery);
		return false;
	}

	return true;
}

int tdb_check(struct tdb_context *tdb,
	      int (*check)(TDB_DATA key, TDB_DATA data, void *private_data),
	      void *private_data)
{
	tdb_off_t *free = NULL, *used = NULL, flist, recovery;
	size_t num_free = 0, num_used = 0, num_found = 0, num_flists = 0;

	if (tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false) != 0)
		return -1;

	if (tdb_lock_expand(tdb, F_RDLCK) != 0) {
		tdb_allrecord_unlock(tdb, F_RDLCK);
		return -1;
	}

	if (!check_header(tdb, &recovery))
		goto fail;

	/* First we do a linear scan, checking all records. */
	if (!check_linear(tdb, &used, &num_used, &free, &num_free, recovery))
		goto fail;

	for (flist = first_flist(tdb); flist; flist = next_flist(tdb, flist)) {
		if (flist == TDB_OFF_ERR)
			goto fail;
		if (!check_free_list(tdb, flist, num_flists, free, num_free,
				     &num_found))
			goto fail;
		num_flists++;
	}

	/* FIXME: Check key uniqueness? */
	if (!check_hash(tdb, used, num_used, num_flists, check, private_data))
		goto fail;

	if (num_found != num_free) {
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "tdb_check: Not all entries are in free table\n");
		return -1;
	}

	tdb_allrecord_unlock(tdb, F_RDLCK);
	tdb_unlock_expand(tdb, F_RDLCK);
	return 0;

fail:
	tdb_allrecord_unlock(tdb, F_RDLCK);
	tdb_unlock_expand(tdb, F_RDLCK);
	return -1;
}
