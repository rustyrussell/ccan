 /*
   Trivial Database 2: hash handling
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
#include <assert.h>

uint64_t tdb_hash(struct tdb_context *tdb, const void *ptr, size_t len)
{
	return tdb->hash_fn(ptr, len, tdb->hash_seed, tdb->hash_data);
}

uint64_t hash_record(struct tdb_context *tdb, tdb_off_t off)
{
	const struct tdb_used_record *r;
	const void *key;
	uint64_t klen, hash;

	r = tdb_access_read(tdb, off, sizeof(*r), true);
	if (TDB_PTR_IS_ERR(r)) {
		/* FIXME */
		return 0;
	}

	klen = rec_key_length(r);
	tdb_access_release(tdb, r);

	key = tdb_access_read(tdb, off + sizeof(*r), klen, false);
	if (TDB_PTR_IS_ERR(key)) {
		return 0;
	}

	hash = tdb_hash(tdb, key, klen);
	tdb_access_release(tdb, key);
	return hash;
}

/* Get bits from a value. */
static uint32_t bits_from(uint64_t val, unsigned start, unsigned num)
{
	assert(num <= 32);
	return (val >> start) & ((1U << num) - 1);
}

/* We take bits from the top: that way we can lock whole sections of the hash
 * by using lock ranges. */
static uint32_t use_bits(struct hash_info *h, unsigned num)
{
	h->hash_used += num;
	return bits_from(h->h, 64 - h->hash_used, num);
}

static tdb_bool_err key_matches(struct tdb_context *tdb,
				const struct tdb_used_record *rec,
				tdb_off_t off,
				const struct tdb_data *key)
{
	tdb_bool_err ret = false;
	const char *rkey;

	if (rec_key_length(rec) != key->dsize) {
		tdb->stats.compare_wrong_keylen++;
		return ret;
	}

	rkey = tdb_access_read(tdb, off + sizeof(*rec), key->dsize, false);
	if (TDB_PTR_IS_ERR(rkey)) {
		return TDB_PTR_ERR(rkey);
	}
	if (memcmp(rkey, key->dptr, key->dsize) == 0)
		ret = true;
	else
		tdb->stats.compare_wrong_keycmp++;
	tdb_access_release(tdb, rkey);
	return ret;
}

/* Does entry match? */
static tdb_bool_err match(struct tdb_context *tdb,
			  struct hash_info *h,
			  const struct tdb_data *key,
			  tdb_off_t val,
			  struct tdb_used_record *rec)
{
	tdb_off_t off;
	enum TDB_ERROR ecode;

	tdb->stats.compares++;
	/* Desired bucket must match. */
	if (h->home_bucket != (val & TDB_OFF_HASH_GROUP_MASK)) {
		tdb->stats.compare_wrong_bucket++;
		return false;
	}

	/* Top bits of offset == next bits of hash. */
	if (bits_from(val, TDB_OFF_HASH_EXTRA_BIT, TDB_OFF_UPPER_STEAL_EXTRA)
	    != bits_from(h->h, 64 - h->hash_used - TDB_OFF_UPPER_STEAL_EXTRA,
		    TDB_OFF_UPPER_STEAL_EXTRA)) {
		tdb->stats.compare_wrong_offsetbits++;
		return false;
	}

	off = val & TDB_OFF_MASK;
	ecode = tdb_read_convert(tdb, off, rec, sizeof(*rec));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if ((h->h & ((1 << 11)-1)) != rec_hash(rec)) {
		tdb->stats.compare_wrong_rechash++;
		return false;
	}

	return key_matches(tdb, rec, off, key);
}

static tdb_off_t hbucket_off(tdb_off_t group_start, unsigned bucket)
{
	return group_start
		+ (bucket % (1 << TDB_HASH_GROUP_BITS)) * sizeof(tdb_off_t);
}

bool is_subhash(tdb_off_t val)
{
	return (val >> TDB_OFF_UPPER_STEAL_SUBHASH_BIT) & 1;
}

/* FIXME: Guess the depth, don't over-lock! */
static tdb_off_t hlock_range(tdb_off_t group, tdb_off_t *size)
{
	*size = 1ULL << (64 - (TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS));
	return group << (64 - (TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS));
}

static tdb_off_t COLD find_in_chain(struct tdb_context *tdb,
				    struct tdb_data key,
				    tdb_off_t chain,
				    struct hash_info *h,
				    struct tdb_used_record *rec,
				    struct traverse_info *tinfo)
{
	tdb_off_t off, next;
	enum TDB_ERROR ecode;

	/* In case nothing is free, we set these to zero. */
	h->home_bucket = h->found_bucket = 0;

	for (off = chain; off; off = next) {
		unsigned int i;

		h->group_start = off;
		ecode = tdb_read_convert(tdb, off, h->group, sizeof(h->group));
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}

		for (i = 0; i < (1 << TDB_HASH_GROUP_BITS); i++) {
			tdb_off_t recoff;
			if (!h->group[i]) {
				/* Remember this empty bucket. */
				h->home_bucket = h->found_bucket = i;
				continue;
			}

			/* We can insert extra bits via add_to_hash
			 * empty bucket logic. */
			recoff = h->group[i] & TDB_OFF_MASK;
			ecode = tdb_read_convert(tdb, recoff, rec,
						 sizeof(*rec));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}

			ecode = key_matches(tdb, rec, recoff, &key);
			if (ecode < 0) {
				return ecode;
			}
			if (ecode == 1) {
				h->home_bucket = h->found_bucket = i;

				if (tinfo) {
					tinfo->levels[tinfo->num_levels]
						.hashtable = off;
					tinfo->levels[tinfo->num_levels]
						.total_buckets
						= 1 << TDB_HASH_GROUP_BITS;
					tinfo->levels[tinfo->num_levels].entry
						= i;
					tinfo->num_levels++;
				}
				return recoff;
			}
		}
		next = tdb_read_off(tdb, off
				    + offsetof(struct tdb_chain, next));
		if (TDB_OFF_IS_ERR(next)) {
			return next;
		}
		if (next)
			next += sizeof(struct tdb_used_record);
	}
	return 0;
}

/* This is the core routine which searches the hashtable for an entry.
 * On error, no locks are held and -ve is returned.
 * Otherwise, hinfo is filled in (and the optional tinfo).
 * If not found, the return value is 0.
 * If found, the return value is the offset, and *rec is the record. */
tdb_off_t find_and_lock(struct tdb_context *tdb,
			struct tdb_data key,
			int ltype,
			struct hash_info *h,
			struct tdb_used_record *rec,
			struct traverse_info *tinfo)
{
	uint32_t i, group;
	tdb_off_t hashtable;
	enum TDB_ERROR ecode;

	h->h = tdb_hash(tdb, key.dptr, key.dsize);
	h->hash_used = 0;
	group = use_bits(h, TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS);
	h->home_bucket = use_bits(h, TDB_HASH_GROUP_BITS);

	h->hlock_start = hlock_range(group, &h->hlock_range);
	ecode = tdb_lock_hashes(tdb, h->hlock_start, h->hlock_range, ltype,
				TDB_LOCK_WAIT);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	hashtable = offsetof(struct tdb_header, hashtable);
	if (tinfo) {
		tinfo->toplevel_group = group;
		tinfo->num_levels = 1;
		tinfo->levels[0].entry = 0;
		tinfo->levels[0].hashtable = hashtable
			+ (group << TDB_HASH_GROUP_BITS) * sizeof(tdb_off_t);
		tinfo->levels[0].total_buckets = 1 << TDB_HASH_GROUP_BITS;
	}

	while (h->hash_used <= 64) {
		/* Read in the hash group. */
		h->group_start = hashtable
			+ group * (sizeof(tdb_off_t) << TDB_HASH_GROUP_BITS);

		ecode = tdb_read_convert(tdb, h->group_start, &h->group,
					 sizeof(h->group));
		if (ecode != TDB_SUCCESS) {
			goto fail;
		}

		/* Pointer to another hash table?  Go down... */
		if (is_subhash(h->group[h->home_bucket])) {
			hashtable = (h->group[h->home_bucket] & TDB_OFF_MASK)
				+ sizeof(struct tdb_used_record);
			if (tinfo) {
				/* When we come back, use *next* bucket */
				tinfo->levels[tinfo->num_levels-1].entry
					+= h->home_bucket + 1;
			}
			group = use_bits(h, TDB_SUBLEVEL_HASH_BITS
					 - TDB_HASH_GROUP_BITS);
			h->home_bucket = use_bits(h, TDB_HASH_GROUP_BITS);
			if (tinfo) {
				tinfo->levels[tinfo->num_levels].hashtable
					= hashtable;
				tinfo->levels[tinfo->num_levels].total_buckets
					= 1 << TDB_SUBLEVEL_HASH_BITS;
				tinfo->levels[tinfo->num_levels].entry
					= group << TDB_HASH_GROUP_BITS;
				tinfo->num_levels++;
			}
			continue;
		}

		/* It's in this group: search (until 0 or all searched) */
		for (i = 0, h->found_bucket = h->home_bucket;
		     i < (1 << TDB_HASH_GROUP_BITS);
		     i++, h->found_bucket = ((h->found_bucket+1)
					     % (1 << TDB_HASH_GROUP_BITS))) {
			tdb_bool_err berr;
			if (is_subhash(h->group[h->found_bucket]))
				continue;

			if (!h->group[h->found_bucket])
				break;

			berr = match(tdb, h, &key, h->group[h->found_bucket],
				     rec);
			if (berr < 0) {
				ecode = berr;
				goto fail;
			}
			if (berr) {
				if (tinfo) {
					tinfo->levels[tinfo->num_levels-1].entry
						+= h->found_bucket;
				}
				return h->group[h->found_bucket] & TDB_OFF_MASK;
			}
		}
		/* Didn't find it: h indicates where it would go. */
		return 0;
	}

	return find_in_chain(tdb, key, hashtable, h, rec, tinfo);

fail:
	tdb_unlock_hashes(tdb, h->hlock_start, h->hlock_range, ltype);
	return ecode;
}

/* I wrote a simple test, expanding a hash to 2GB, for the following
 * cases:
 * 1) Expanding all the buckets at once,
 * 2) Expanding the bucket we wanted to place the new entry into.
 * 3) Expanding the most-populated bucket,
 *
 * I measured the worst/average/best density during this process.
 * 1) 3%/16%/30%
 * 2) 4%/20%/38%
 * 3) 6%/22%/41%
 *
 * So we figure out the busiest bucket for the moment.
 */
static unsigned fullest_bucket(struct tdb_context *tdb,
			       const tdb_off_t *group,
			       unsigned new_bucket)
{
	unsigned counts[1 << TDB_HASH_GROUP_BITS] = { 0 };
	unsigned int i, best_bucket;

	/* Count the new entry. */
	counts[new_bucket]++;
	best_bucket = new_bucket;

	for (i = 0; i < (1 << TDB_HASH_GROUP_BITS); i++) {
		unsigned this_bucket;

		if (is_subhash(group[i]))
			continue;
		this_bucket = group[i] & TDB_OFF_HASH_GROUP_MASK;
		if (++counts[this_bucket] > counts[best_bucket])
			best_bucket = this_bucket;
	}

	return best_bucket;
}

static bool put_into_group(tdb_off_t *group,
			   unsigned bucket, tdb_off_t encoded)
{
	unsigned int i;

	for (i = 0; i < (1 << TDB_HASH_GROUP_BITS); i++) {
		unsigned b = (bucket + i) % (1 << TDB_HASH_GROUP_BITS);

		if (group[b] == 0) {
			group[b] = encoded;
			return true;
		}
	}
	return false;
}

static void force_into_group(tdb_off_t *group,
			     unsigned bucket, tdb_off_t encoded)
{
	if (!put_into_group(group, bucket, encoded))
		abort();
}

static tdb_off_t encode_offset(tdb_off_t new_off, struct hash_info *h)
{
	return h->home_bucket
		| new_off
		| ((uint64_t)bits_from(h->h,
				  64 - h->hash_used - TDB_OFF_UPPER_STEAL_EXTRA,
				  TDB_OFF_UPPER_STEAL_EXTRA)
		   << TDB_OFF_HASH_EXTRA_BIT);
}

/* Simply overwrite the hash entry we found before. */
enum TDB_ERROR replace_in_hash(struct tdb_context *tdb,
			       struct hash_info *h,
			       tdb_off_t new_off)
{
	return tdb_write_off(tdb, hbucket_off(h->group_start, h->found_bucket),
			     encode_offset(new_off, h));
}

/* We slot in anywhere that's empty in the chain. */
static enum TDB_ERROR COLD add_to_chain(struct tdb_context *tdb,
					tdb_off_t subhash,
					tdb_off_t new_off)
{
	tdb_off_t entry;
	enum TDB_ERROR ecode;

	entry = tdb_find_zero_off(tdb, subhash, 1<<TDB_HASH_GROUP_BITS);
	if (TDB_OFF_IS_ERR(entry)) {
		return entry;
	}

	if (entry == 1 << TDB_HASH_GROUP_BITS) {
		tdb_off_t next;

		next = tdb_read_off(tdb, subhash
				    + offsetof(struct tdb_chain, next));
		if (TDB_OFF_IS_ERR(next)) {
			return next;
		}

		if (!next) {
			next = alloc(tdb, 0, sizeof(struct tdb_chain), 0,
				     TDB_CHAIN_MAGIC, false);
			if (TDB_OFF_IS_ERR(next))
				return next;
			ecode = zero_out(tdb,
					 next+sizeof(struct tdb_used_record),
					 sizeof(struct tdb_chain));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
			ecode = tdb_write_off(tdb, subhash
					      + offsetof(struct tdb_chain,
							 next),
					      next);
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
		}
		return add_to_chain(tdb, next, new_off);
	}

	return tdb_write_off(tdb, subhash + entry * sizeof(tdb_off_t),
			     new_off);
}

/* Add into a newly created subhash. */
static enum TDB_ERROR add_to_subhash(struct tdb_context *tdb, tdb_off_t subhash,
				     unsigned hash_used, tdb_off_t val)
{
	tdb_off_t off = (val & TDB_OFF_MASK), *group;
	struct hash_info h;
	unsigned int gnum;

	h.hash_used = hash_used;

	if (hash_used + TDB_SUBLEVEL_HASH_BITS > 64)
		return add_to_chain(tdb, subhash, off);

	h.h = hash_record(tdb, off);
	gnum = use_bits(&h, TDB_SUBLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS);
	h.group_start = subhash
		+ gnum * (sizeof(tdb_off_t) << TDB_HASH_GROUP_BITS);
	h.home_bucket = use_bits(&h, TDB_HASH_GROUP_BITS);

	group = tdb_access_write(tdb, h.group_start,
				 sizeof(*group) << TDB_HASH_GROUP_BITS, true);
	if (TDB_PTR_IS_ERR(group)) {
		return TDB_PTR_ERR(group);
	}
	force_into_group(group, h.home_bucket, encode_offset(off, &h));
	return tdb_access_commit(tdb, group);
}

static enum TDB_ERROR expand_group(struct tdb_context *tdb, struct hash_info *h)
{
	unsigned bucket, num_vals, i, magic;
	size_t subsize;
	tdb_off_t subhash;
	tdb_off_t vals[1 << TDB_HASH_GROUP_BITS];
	enum TDB_ERROR ecode;

	/* Attach new empty subhash under fullest bucket. */
	bucket = fullest_bucket(tdb, h->group, h->home_bucket);

	if (h->hash_used == 64) {
		tdb->stats.alloc_chain++;
		subsize = sizeof(struct tdb_chain);
		magic = TDB_CHAIN_MAGIC;
	} else {
		tdb->stats.alloc_subhash++;
		subsize = (sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS);
		magic = TDB_HTABLE_MAGIC;
	}

	subhash = alloc(tdb, 0, subsize, 0, magic, false);
	if (TDB_OFF_IS_ERR(subhash)) {
		return subhash;
	}

	ecode = zero_out(tdb, subhash + sizeof(struct tdb_used_record),
			 subsize);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* Remove any which are destined for bucket or are in wrong place. */
	num_vals = 0;
	for (i = 0; i < (1 << TDB_HASH_GROUP_BITS); i++) {
		unsigned home_bucket = h->group[i] & TDB_OFF_HASH_GROUP_MASK;
		if (!h->group[i] || is_subhash(h->group[i]))
			continue;
		if (home_bucket == bucket || home_bucket != i) {
			vals[num_vals++] = h->group[i];
			h->group[i] = 0;
		}
	}
	/* FIXME: This assert is valid, but we do this during unit test :( */
	/* assert(num_vals); */

	/* Overwrite expanded bucket with subhash pointer. */
	h->group[bucket] = subhash | (1ULL << TDB_OFF_UPPER_STEAL_SUBHASH_BIT);

	/* Point to actual contents of record. */
	subhash += sizeof(struct tdb_used_record);

	/* Put values back. */
	for (i = 0; i < num_vals; i++) {
		unsigned this_bucket = vals[i] & TDB_OFF_HASH_GROUP_MASK;

		if (this_bucket == bucket) {
			ecode = add_to_subhash(tdb, subhash, h->hash_used,
					       vals[i]);
			if (ecode != TDB_SUCCESS)
				return ecode;
		} else {
			/* There should be room to put this back. */
			force_into_group(h->group, this_bucket, vals[i]);
		}
	}
	return TDB_SUCCESS;
}

enum TDB_ERROR delete_from_hash(struct tdb_context *tdb, struct hash_info *h)
{
	unsigned int i, num_movers = 0;
	tdb_off_t movers[1 << TDB_HASH_GROUP_BITS];

	h->group[h->found_bucket] = 0;
	for (i = 1; i < (1 << TDB_HASH_GROUP_BITS); i++) {
		unsigned this_bucket;

		this_bucket = (h->found_bucket+i) % (1 << TDB_HASH_GROUP_BITS);
		/* Empty bucket?  We're done. */
		if (!h->group[this_bucket])
			break;

		/* Ignore subhashes. */
		if (is_subhash(h->group[this_bucket]))
			continue;

		/* If this one is not happy where it is, we'll move it. */
		if ((h->group[this_bucket] & TDB_OFF_HASH_GROUP_MASK)
		    != this_bucket) {
			movers[num_movers++] = h->group[this_bucket];
			h->group[this_bucket] = 0;
		}
	}

	/* Put back the ones we erased. */
	for (i = 0; i < num_movers; i++) {
		force_into_group(h->group, movers[i] & TDB_OFF_HASH_GROUP_MASK,
				 movers[i]);
	}

	/* Now we write back the hash group */
	return tdb_write_convert(tdb, h->group_start,
				 h->group, sizeof(h->group));
}

enum TDB_ERROR add_to_hash(struct tdb_context *tdb, struct hash_info *h,
			   tdb_off_t new_off)
{
	enum TDB_ERROR ecode;

	/* We hit an empty bucket during search?  That's where it goes. */
	if (!h->group[h->found_bucket]) {
		h->group[h->found_bucket] = encode_offset(new_off, h);
		/* Write back the modified group. */
		return tdb_write_convert(tdb, h->group_start,
					 h->group, sizeof(h->group));
	}

	if (h->hash_used > 64)
		return add_to_chain(tdb, h->group_start, new_off);

	/* We're full.  Expand. */
	ecode = expand_group(tdb, h);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (is_subhash(h->group[h->home_bucket])) {
		/* We were expanded! */
		tdb_off_t hashtable;
		unsigned int gnum;

		/* Write back the modified group. */
		ecode = tdb_write_convert(tdb, h->group_start, h->group,
					  sizeof(h->group));
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}

		/* Move hashinfo down a level. */
		hashtable = (h->group[h->home_bucket] & TDB_OFF_MASK)
			+ sizeof(struct tdb_used_record);
		gnum = use_bits(h,TDB_SUBLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS);
		h->home_bucket = use_bits(h, TDB_HASH_GROUP_BITS);
		h->group_start = hashtable
			+ gnum * (sizeof(tdb_off_t) << TDB_HASH_GROUP_BITS);
		ecode = tdb_read_convert(tdb, h->group_start, &h->group,
					 sizeof(h->group));
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
	}

	/* Expanding the group must have made room if it didn't choose this
	 * bucket. */
	if (put_into_group(h->group, h->home_bucket, encode_offset(new_off,h))){
		return tdb_write_convert(tdb, h->group_start,
					 h->group, sizeof(h->group));
	}

	/* This can happen if all hashes in group (and us) dropped into same
	 * group in subhash. */
	return add_to_hash(tdb, h, new_off);
}

/* Traverse support: returns offset of record, or 0 or -ve error. */
static tdb_off_t iterate_hash(struct tdb_context *tdb,
			      struct traverse_info *tinfo)
{
	tdb_off_t off, val, i;
	struct traverse_level *tlevel;

	tlevel = &tinfo->levels[tinfo->num_levels-1];

again:
	for (i = tdb_find_nonzero_off(tdb, tlevel->hashtable,
				      tlevel->entry, tlevel->total_buckets);
	     i != tlevel->total_buckets;
	     i = tdb_find_nonzero_off(tdb, tlevel->hashtable,
				      i+1, tlevel->total_buckets)) {
		if (TDB_OFF_IS_ERR(i)) {
			return i;
		}

		val = tdb_read_off(tdb, tlevel->hashtable+sizeof(tdb_off_t)*i);
		if (TDB_OFF_IS_ERR(val)) {
			return val;
		}

		off = val & TDB_OFF_MASK;

		/* This makes the delete-all-in-traverse case work
		 * (and simplifies our logic a little). */
		if (off == tinfo->prev)
			continue;

		tlevel->entry = i;

		if (!is_subhash(val)) {
			/* Found one. */
			tinfo->prev = off;
			return off;
		}

		/* When we come back, we want the next one */
		tlevel->entry++;
		tinfo->num_levels++;
		tlevel++;
		tlevel->hashtable = off + sizeof(struct tdb_used_record);
		tlevel->entry = 0;
		/* Next level is a chain? */
		if (unlikely(tinfo->num_levels == TDB_MAX_LEVELS + 1))
			tlevel->total_buckets = (1 << TDB_HASH_GROUP_BITS);
		else
			tlevel->total_buckets = (1 << TDB_SUBLEVEL_HASH_BITS);
		goto again;
	}

	/* Nothing there? */
	if (tinfo->num_levels == 1)
		return 0;

	/* Handle chained entries. */
	if (unlikely(tinfo->num_levels == TDB_MAX_LEVELS + 1)) {
		tlevel->hashtable = tdb_read_off(tdb, tlevel->hashtable
						 + offsetof(struct tdb_chain,
							    next));
		if (TDB_OFF_IS_ERR(tlevel->hashtable)) {
			return tlevel->hashtable;
		}
		if (tlevel->hashtable) {
			tlevel->hashtable += sizeof(struct tdb_used_record);
			tlevel->entry = 0;
			goto again;
		}
	}

	/* Go back up and keep searching. */
	tinfo->num_levels--;
	tlevel--;
	goto again;
}

/* Return success if we find something, TDB_ERR_NOEXIST if none. */
enum TDB_ERROR next_in_hash(struct tdb_context *tdb,
			    struct traverse_info *tinfo,
			    TDB_DATA *kbuf, size_t *dlen)
{
	const unsigned group_bits = TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS;
	tdb_off_t hl_start, hl_range, off;
	enum TDB_ERROR ecode;

	while (tinfo->toplevel_group < (1 << group_bits)) {
		hl_start = (tdb_off_t)tinfo->toplevel_group
			<< (64 - group_bits);
		hl_range = 1ULL << group_bits;
		ecode = tdb_lock_hashes(tdb, hl_start, hl_range, F_RDLCK,
					TDB_LOCK_WAIT);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}

		off = iterate_hash(tdb, tinfo);
		if (off) {
			struct tdb_used_record rec;

			if (TDB_OFF_IS_ERR(off)) {
				ecode = off;
				goto fail;
			}

			ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec));
			if (ecode != TDB_SUCCESS) {
				goto fail;
			}
			if (rec_magic(&rec) != TDB_USED_MAGIC) {
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "next_in_hash:"
						   " corrupt record at %llu",
						   (long long)off);
				goto fail;
			}

			kbuf->dsize = rec_key_length(&rec);

			/* They want data as well? */
			if (dlen) {
				*dlen = rec_data_length(&rec);
				kbuf->dptr = tdb_alloc_read(tdb,
							    off + sizeof(rec),
							    kbuf->dsize
							    + *dlen);
			} else {
				kbuf->dptr = tdb_alloc_read(tdb,
							    off + sizeof(rec),
							    kbuf->dsize);
			}
			tdb_unlock_hashes(tdb, hl_start, hl_range, F_RDLCK);
			if (TDB_PTR_IS_ERR(kbuf->dptr)) {
				return TDB_PTR_ERR(kbuf->dptr);
			}
			return TDB_SUCCESS;
		}

		tdb_unlock_hashes(tdb, hl_start, hl_range, F_RDLCK);

		tinfo->toplevel_group++;
		tinfo->levels[0].hashtable
			+= (sizeof(tdb_off_t) << TDB_HASH_GROUP_BITS);
		tinfo->levels[0].entry = 0;
	}
	return TDB_ERR_NOEXIST;

fail:
	tdb_unlock_hashes(tdb, hl_start, hl_range, F_RDLCK);
	return ecode;

}

enum TDB_ERROR first_in_hash(struct tdb_context *tdb,
			     struct traverse_info *tinfo,
			     TDB_DATA *kbuf, size_t *dlen)
{
	tinfo->prev = 0;
	tinfo->toplevel_group = 0;
	tinfo->num_levels = 1;
	tinfo->levels[0].hashtable = offsetof(struct tdb_header, hashtable);
	tinfo->levels[0].entry = 0;
	tinfo->levels[0].total_buckets = (1 << TDB_HASH_GROUP_BITS);

	return next_in_hash(tdb, tinfo, kbuf, dlen);
}

/* Even if the entry isn't in this hash bucket, you'd have to lock this
 * bucket to find it. */
static enum TDB_ERROR chainlock(struct tdb_context *tdb, const TDB_DATA *key,
				int ltype, enum tdb_lock_flags waitflag,
				const char *func)
{
	enum TDB_ERROR ecode;
	uint64_t h = tdb_hash(tdb, key->dptr, key->dsize);
	tdb_off_t lockstart, locksize;
	unsigned int group, gbits;

	gbits = TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS;
	group = bits_from(h, 64 - gbits, gbits);

	lockstart = hlock_range(group, &locksize);

	ecode = tdb_lock_hashes(tdb, lockstart, locksize, ltype, waitflag);
	tdb_trace_1rec(tdb, func, *key);
	return ecode;
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
enum TDB_ERROR tdb_chainlock(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb->last_error = chainlock(tdb, &key, F_WRLCK, TDB_LOCK_WAIT,
					   "tdb_chainlock");
}

void tdb_chainunlock(struct tdb_context *tdb, TDB_DATA key)
{
	uint64_t h = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_off_t lockstart, locksize;
	unsigned int group, gbits;

	gbits = TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS;
	group = bits_from(h, 64 - gbits, gbits);

	lockstart = hlock_range(group, &locksize);

	tdb_trace_1rec(tdb, "tdb_chainunlock", key);
	tdb_unlock_hashes(tdb, lockstart, locksize, F_WRLCK);
}

enum TDB_ERROR tdb_chainlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	return tdb->last_error = chainlock(tdb, &key, F_RDLCK, TDB_LOCK_WAIT,
					   "tdb_chainlock_read");
}

void tdb_chainunlock_read(struct tdb_context *tdb, TDB_DATA key)
{
	uint64_t h = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_off_t lockstart, locksize;
	unsigned int group, gbits;

	gbits = TDB_TOPLEVEL_HASH_BITS - TDB_HASH_GROUP_BITS;
	group = bits_from(h, 64 - gbits, gbits);

	lockstart = hlock_range(group, &locksize);

	tdb_trace_1rec(tdb, "tdb_chainunlock_read", key);
	tdb_unlock_hashes(tdb, lockstart, locksize, F_RDLCK);
}
