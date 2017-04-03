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
#include <ccan/hash/hash.h>

/* Default hash function. */
uint32_t ntdb_jenkins_hash(const void *key, size_t length, uint32_t seed,
			  void *unused)
{
	return hash_stable((const unsigned char *)key, length, seed);
}

uint32_t ntdb_hash(struct ntdb_context *ntdb, const void *ptr, size_t len)
{
	return ntdb->hash_fn(ptr, len, ntdb->hash_seed, ntdb->hash_data);
}

static ntdb_bool_err key_matches(struct ntdb_context *ntdb,
				 const struct ntdb_used_record *rec,
				 ntdb_off_t off,
				 const NTDB_DATA *key,
				 const char **rptr)
{
	ntdb_bool_err ret = false;
	const char *rkey;

	if (rec_key_length(rec) != key->dsize) {
		ntdb->stats.compare_wrong_keylen++;
		return ret;
	}

	rkey = ntdb_access_read(ntdb, off + sizeof(*rec),
				key->dsize + rec_data_length(rec), false);
	if (NTDB_PTR_IS_ERR(rkey)) {
		return (ntdb_bool_err)NTDB_PTR_ERR(rkey);
	}
	if (memcmp(rkey, key->dptr, key->dsize) == 0) {
		if (rptr) {
			*rptr = rkey;
		} else {
			ntdb_access_release(ntdb, rkey);
		}
		return true;
	}
	ntdb->stats.compare_wrong_keycmp++;
	ntdb_access_release(ntdb, rkey);
	return ret;
}

/* Does entry match? */
static ntdb_bool_err match(struct ntdb_context *ntdb,
			   uint32_t hash,
			   const NTDB_DATA *key,
			   ntdb_off_t val,
			   struct ntdb_used_record *rec,
			   const char **rptr)
{
	ntdb_off_t off;
	enum NTDB_ERROR ecode;

	ntdb->stats.compares++;

	/* Top bits of offset == next bits of hash. */
	if (bits_from(hash, ntdb->hash_bits, NTDB_OFF_UPPER_STEAL)
	    != bits_from(val, 64-NTDB_OFF_UPPER_STEAL, NTDB_OFF_UPPER_STEAL)) {
		ntdb->stats.compare_wrong_offsetbits++;
		return false;
	}

	off = val & NTDB_OFF_MASK;
	ecode = ntdb_read_convert(ntdb, off, rec, sizeof(*rec));
	if (ecode != NTDB_SUCCESS) {
		return (ntdb_bool_err)ecode;
	}

	return key_matches(ntdb, rec, off, key, rptr);
}

static bool is_chain(ntdb_off_t val)
{
	return val & (1ULL << NTDB_OFF_CHAIN_BIT);
}

static ntdb_off_t hbucket_off(ntdb_off_t base, ntdb_len_t idx)
{
	return base + sizeof(struct ntdb_used_record)
		+ idx * sizeof(ntdb_off_t);
}

/* This is the core routine which searches the hashtable for an entry.
 * On error, no locks are held and -ve is returned.
 * Otherwise, hinfo is filled in.
 * If not found, the return value is 0.
 * If found, the return value is the offset, and *rec is the record. */
ntdb_off_t find_and_lock(struct ntdb_context *ntdb,
			 NTDB_DATA key,
			 int ltype,
			 struct hash_info *h,
			 struct ntdb_used_record *rec,
			 const char **rptr)
{
	ntdb_off_t off, val;
	const ntdb_off_t *arr = NULL;
	ntdb_len_t i;
	bool found_empty;
	enum NTDB_ERROR ecode;
	struct ntdb_used_record chdr;
	ntdb_bool_err berr;

	h->h = ntdb_hash(ntdb, key.dptr, key.dsize);

	h->table = NTDB_HASH_OFFSET;
	h->table_size = 1 << ntdb->hash_bits;
	h->bucket = bits_from(h->h, 0, ntdb->hash_bits);
	h->old_val = 0;

	ecode = ntdb_lock_hash(ntdb, h->bucket, ltype);
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
	}

	off = hbucket_off(h->table, h->bucket);
	val = ntdb_read_off(ntdb, off);
	if (NTDB_OFF_IS_ERR(val)) {
		ecode = NTDB_OFF_TO_ERR(val);
		goto fail;
	}

	/* Directly in hash table? */
	if (!likely(is_chain(val))) {
		if (val) {
			berr = match(ntdb, h->h, &key, val, rec, rptr);
			if (berr < 0) {
				ecode = NTDB_OFF_TO_ERR(berr);
				goto fail;
			}
			if (berr) {
				return val & NTDB_OFF_MASK;
			}
			/* If you want to insert here, make a chain. */
			h->old_val = val;
		}
		return 0;
	}

	/* Nope?  Iterate through chain. */
	h->table = val & NTDB_OFF_MASK;

	ecode = ntdb_read_convert(ntdb, h->table, &chdr, sizeof(chdr));
	if (ecode != NTDB_SUCCESS) {
		goto fail;
	}

	if (rec_magic(&chdr) != NTDB_CHAIN_MAGIC) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT,
				    NTDB_LOG_ERROR,
				    "find_and_lock:"
				    " corrupt record %#x at %llu",
				    rec_magic(&chdr), (long long)off);
		goto fail;
	}

	h->table_size = rec_data_length(&chdr) / sizeof(ntdb_off_t);

	arr = ntdb_access_read(ntdb, hbucket_off(h->table, 0),
			       rec_data_length(&chdr), true);
	if (NTDB_PTR_IS_ERR(arr)) {
		ecode = NTDB_PTR_ERR(arr);
		goto fail;
	}

	found_empty = false;
	for (i = 0; i < h->table_size; i++) {
		if (arr[i] == 0) {
			if (!found_empty) {
				h->bucket = i;
				found_empty = true;
			}
		} else {
			berr = match(ntdb, h->h, &key, arr[i], rec, rptr);
			if (berr < 0) {
				ecode = NTDB_OFF_TO_ERR(berr);
				ntdb_access_release(ntdb, arr);
				goto fail;
			}
			if (berr) {
				/* We found it! */
				h->bucket = i;
				off = arr[i] & NTDB_OFF_MASK;
				ntdb_access_release(ntdb, arr);
				return off;
			}
		}
	}
	if (!found_empty) {
		/* Set to any non-zero value */
		h->old_val = 1;
		h->bucket = i;
	}

	ntdb_access_release(ntdb, arr);
	return 0;

fail:
	ntdb_unlock_hash(ntdb, h->bucket, ltype);
	return NTDB_ERR_TO_OFF(ecode);
}

static ntdb_off_t encode_offset(const struct ntdb_context *ntdb,
				ntdb_off_t new_off, uint32_t hash)
{
	ntdb_off_t extra;

	assert((new_off & (1ULL << NTDB_OFF_CHAIN_BIT)) == 0);
	assert((new_off >> (64 - NTDB_OFF_UPPER_STEAL)) == 0);
	/* We pack extra hash bits into the upper bits of the offset. */
	extra = bits_from(hash, ntdb->hash_bits, NTDB_OFF_UPPER_STEAL);
	extra <<= (64 - NTDB_OFF_UPPER_STEAL);

	return new_off | extra;
}

/* Simply overwrite the hash entry we found before. */
enum NTDB_ERROR replace_in_hash(struct ntdb_context *ntdb,
				const struct hash_info *h,
				ntdb_off_t new_off)
{
	return ntdb_write_off(ntdb, hbucket_off(h->table, h->bucket),
			      encode_offset(ntdb, new_off, h->h));
}

enum NTDB_ERROR delete_from_hash(struct ntdb_context *ntdb,
				 const struct hash_info *h)
{
	return ntdb_write_off(ntdb, hbucket_off(h->table, h->bucket), 0);
}


enum NTDB_ERROR add_to_hash(struct ntdb_context *ntdb,
			    const struct hash_info *h,
			    ntdb_off_t new_off)
{
	enum NTDB_ERROR ecode;
	ntdb_off_t chain;
	struct ntdb_used_record chdr;
	const ntdb_off_t *old;
	ntdb_off_t *new;

	/* We hit an empty bucket during search?  That's where it goes. */
	if (!h->old_val) {
		return replace_in_hash(ntdb, h, new_off);
	}

	/* Full at top-level?  Create a 2-element chain. */
	if (h->table == NTDB_HASH_OFFSET) {
		ntdb_off_t pair[2];

		/* One element is old value, the other is the new value. */
		pair[0] = h->old_val;
		pair[1] = encode_offset(ntdb, new_off, h->h);

		chain = alloc(ntdb, 0, sizeof(pair), NTDB_CHAIN_MAGIC, true);
		if (NTDB_OFF_IS_ERR(chain)) {
			return NTDB_OFF_TO_ERR(chain);
		}
		ecode = ntdb_write_convert(ntdb,
					   chain
					   + sizeof(struct ntdb_used_record),
					   pair, sizeof(pair));
		if (ecode == NTDB_SUCCESS) {
			ecode = ntdb_write_off(ntdb,
					       hbucket_off(h->table, h->bucket),
					       chain
					       | (1ULL << NTDB_OFF_CHAIN_BIT));
		}
		return ecode;
	}

	/* Full bucket.  Expand. */
	ecode = ntdb_read_convert(ntdb, h->table, &chdr, sizeof(chdr));
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (rec_extra_padding(&chdr) >= sizeof(new_off)) {
		/* Expand in place. */
		uint64_t dlen = rec_data_length(&chdr);

		ecode = set_header(ntdb, &chdr, NTDB_CHAIN_MAGIC, 0,
				   dlen + sizeof(new_off),
				   dlen + rec_extra_padding(&chdr));

		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
		/* find_and_lock set up h to point to last bucket. */
		ecode = replace_in_hash(ntdb, h, new_off);
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
		ecode = ntdb_write_convert(ntdb, h->table, &chdr, sizeof(chdr));
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}
		/* For futureproofing, we always make the first byte of padding
		 * a zero. */
		if (rec_extra_padding(&chdr)) {
			ecode = ntdb->io->twrite(ntdb, h->table + sizeof(chdr)
						 + dlen + sizeof(new_off),
						 "", 1);
		}
		return ecode;
	}

	/* We need to reallocate the chain. */
	chain = alloc(ntdb, 0, (h->table_size + 1) * sizeof(ntdb_off_t),
		      NTDB_CHAIN_MAGIC, true);
	if (NTDB_OFF_IS_ERR(chain)) {
		return NTDB_OFF_TO_ERR(chain);
	}

	/* Map both and copy across old buckets. */
	old = ntdb_access_read(ntdb, hbucket_off(h->table, 0),
			       h->table_size*sizeof(ntdb_off_t), true);
	if (NTDB_PTR_IS_ERR(old)) {
		return NTDB_PTR_ERR(old);
	}
	new = ntdb_access_write(ntdb, hbucket_off(chain, 0),
				(h->table_size + 1)*sizeof(ntdb_off_t), true);
	if (NTDB_PTR_IS_ERR(new)) {
		ntdb_access_release(ntdb, old);
		return NTDB_PTR_ERR(new);
	}

	memcpy(new, old, h->bucket * sizeof(ntdb_off_t));
	new[h->bucket] = encode_offset(ntdb, new_off, h->h);
	ntdb_access_release(ntdb, old);

	ecode = ntdb_access_commit(ntdb, new);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* Free the old chain. */
	ecode = add_free_record(ntdb, h->table,
				sizeof(struct ntdb_used_record)
				+ rec_data_length(&chdr)
				+ rec_extra_padding(&chdr),
				NTDB_LOCK_WAIT, true);

	/* Replace top-level to point to new chain */
	return ntdb_write_off(ntdb,
			      hbucket_off(NTDB_HASH_OFFSET,
					  bits_from(h->h, 0, ntdb->hash_bits)),
			      chain | (1ULL << NTDB_OFF_CHAIN_BIT));
}

/* Traverse support: returns offset of record, or 0 or -ve error. */
static ntdb_off_t iterate_chain(struct ntdb_context *ntdb,
				ntdb_off_t val,
				struct hash_info *h)
{
	ntdb_off_t i;
	enum NTDB_ERROR ecode;
	struct ntdb_used_record chdr;

	/* First load up chain header. */
	h->table = val & NTDB_OFF_MASK;
	ecode = ntdb_read_convert(ntdb, h->table, &chdr, sizeof(chdr));
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	if (rec_magic(&chdr) != NTDB_CHAIN_MAGIC) {
		return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT,
				   NTDB_LOG_ERROR,
				   "get_table:"
				   " corrupt record %#x at %llu",
				   rec_magic(&chdr),
				   (long long)h->table);
	}

	/* Chain length is implied by data length. */
	h->table_size = rec_data_length(&chdr) / sizeof(ntdb_off_t);

	i = ntdb_find_nonzero_off(ntdb, hbucket_off(h->table, 0), h->bucket,
				  h->table_size);
	if (NTDB_OFF_IS_ERR(i)) {
		return i;
	}

	if (i != h->table_size) {
		/* Return to next bucket. */
		h->bucket = i + 1;
		val = ntdb_read_off(ntdb, hbucket_off(h->table, i));
		if (NTDB_OFF_IS_ERR(val)) {
			return val;
		}
		return val & NTDB_OFF_MASK;
	}

	/* Go back up to hash table. */
	h->table = NTDB_HASH_OFFSET;
	h->table_size = 1 << ntdb->hash_bits;
	h->bucket = bits_from(h->h, 0, ntdb->hash_bits) + 1;
	return 0;
}

/* Keeps hash locked unless returns 0 or error. */
static ntdb_off_t lock_and_iterate_hash(struct ntdb_context *ntdb,
					struct hash_info *h)
{
	ntdb_off_t val, i;
	enum NTDB_ERROR ecode;

	if (h->table != NTDB_HASH_OFFSET) {
		/* We're in a chain. */
		i = bits_from(h->h, 0, ntdb->hash_bits);
		ecode = ntdb_lock_hash(ntdb, i, F_RDLCK);
		if (ecode != NTDB_SUCCESS) {
			return NTDB_ERR_TO_OFF(ecode);
		}

		/* We dropped lock, bucket might have moved! */
		val = ntdb_read_off(ntdb, hbucket_off(NTDB_HASH_OFFSET, i));
		if (NTDB_OFF_IS_ERR(val)) {
			goto unlock;
		}

		/* We don't remove chains: there should still be one there! */
		if (!val || !is_chain(val)) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT,
					    NTDB_LOG_ERROR,
					    "iterate_hash:"
					    " vanished hchain %llu at %llu",
					    (long long)val,
					    (long long)i);
			val = NTDB_ERR_TO_OFF(ecode);
			goto unlock;
		}

		/* Find next bucket in the chain. */
		val = iterate_chain(ntdb, val, h);
		if (NTDB_OFF_IS_ERR(val)) {
			goto unlock;
		}
		if (val != 0) {
			return val;
		}
		ntdb_unlock_hash(ntdb, i, F_RDLCK);

		/* OK, we've reset h back to top level. */
	}

	/* We do this unlocked, then re-check. */
	for (i = ntdb_find_nonzero_off(ntdb, hbucket_off(h->table, 0),
				       h->bucket, h->table_size);
	     i != h->table_size;
	     i = ntdb_find_nonzero_off(ntdb, hbucket_off(h->table, 0),
				       i+1, h->table_size)) {
		ecode = ntdb_lock_hash(ntdb, i, F_RDLCK);
		if (ecode != NTDB_SUCCESS) {
			return NTDB_ERR_TO_OFF(ecode);
		}

		val = ntdb_read_off(ntdb, hbucket_off(h->table, i));
		if (NTDB_OFF_IS_ERR(val)) {
			goto unlock;
		}

		/* Lost race, and it's empty? */
		if (!val) {
			ntdb->stats.traverse_val_vanished++;
			ntdb_unlock_hash(ntdb, i, F_RDLCK);
			continue;
		}

		if (!is_chain(val)) {
			/* So caller knows what lock to free. */
			h->h = i;
			/* Return to next bucket. */
			h->bucket = i + 1;
			val &= NTDB_OFF_MASK;
			return val;
		}

		/* Start at beginning of chain */
		h->bucket = 0;
		h->h = i;

		val = iterate_chain(ntdb, val, h);
		if (NTDB_OFF_IS_ERR(val)) {
			goto unlock;
		}
		if (val != 0) {
			return val;
		}

		/* Otherwise, bucket has been set to i+1 */
		ntdb_unlock_hash(ntdb, i, F_RDLCK);
	}
	return 0;

unlock:
	ntdb_unlock_hash(ntdb, i, F_RDLCK);
	return val;
}

/* Return success if we find something, NTDB_ERR_NOEXIST if none. */
enum NTDB_ERROR next_in_hash(struct ntdb_context *ntdb,
			     struct hash_info *h,
			     NTDB_DATA *kbuf, size_t *dlen)
{
	ntdb_off_t off;
	struct ntdb_used_record rec;
	enum NTDB_ERROR ecode;

	off = lock_and_iterate_hash(ntdb, h);

	if (NTDB_OFF_IS_ERR(off)) {
		return NTDB_OFF_TO_ERR(off);
	} else if (off == 0) {
		return NTDB_ERR_NOEXIST;
	}

	/* The hash for this key is still locked. */
	ecode = ntdb_read_convert(ntdb, off, &rec, sizeof(rec));
	if (ecode != NTDB_SUCCESS) {
		goto unlock;
	}
	if (rec_magic(&rec) != NTDB_USED_MAGIC) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT,
				    NTDB_LOG_ERROR,
				    "next_in_hash:"
				    " corrupt record at %llu",
				    (long long)off);
		goto unlock;
	}

	kbuf->dsize = rec_key_length(&rec);

	/* They want data as well? */
	if (dlen) {
		*dlen = rec_data_length(&rec);
		kbuf->dptr = ntdb_alloc_read(ntdb, off + sizeof(rec),
					     kbuf->dsize + *dlen);
	} else {
		kbuf->dptr = ntdb_alloc_read(ntdb, off + sizeof(rec),
					     kbuf->dsize);
	}
	if (NTDB_PTR_IS_ERR(kbuf->dptr)) {
		ecode = NTDB_PTR_ERR(kbuf->dptr);
		goto unlock;
	}
	ecode = NTDB_SUCCESS;

unlock:
	ntdb_unlock_hash(ntdb, bits_from(h->h, 0, ntdb->hash_bits), F_RDLCK);
	return ecode;

}

enum NTDB_ERROR first_in_hash(struct ntdb_context *ntdb,
			     struct hash_info *h,
			     NTDB_DATA *kbuf, size_t *dlen)
{
	h->table = NTDB_HASH_OFFSET;
	h->table_size = 1 << ntdb->hash_bits;
	h->bucket = 0;

	return next_in_hash(ntdb, h, kbuf, dlen);
}

/* Even if the entry isn't in this hash bucket, you'd have to lock this
 * bucket to find it. */
static enum NTDB_ERROR chainlock(struct ntdb_context *ntdb,
				 const NTDB_DATA *key, int ltype)
{
	uint32_t h = ntdb_hash(ntdb, key->dptr, key->dsize);

	return ntdb_lock_hash(ntdb, bits_from(h, 0, ntdb->hash_bits), ltype);
}

/* lock/unlock one hash chain. This is meant to be used to reduce
   contention - it cannot guarantee how many records will be locked */
_PUBLIC_ enum NTDB_ERROR ntdb_chainlock(struct ntdb_context *ntdb, NTDB_DATA key)
{
	return chainlock(ntdb, &key, F_WRLCK);
}

_PUBLIC_ void ntdb_chainunlock(struct ntdb_context *ntdb, NTDB_DATA key)
{
	uint32_t h = ntdb_hash(ntdb, key.dptr, key.dsize);

	ntdb_unlock_hash(ntdb, bits_from(h, 0, ntdb->hash_bits), F_WRLCK);
}

_PUBLIC_ enum NTDB_ERROR ntdb_chainlock_read(struct ntdb_context *ntdb,
					     NTDB_DATA key)
{
	return chainlock(ntdb, &key, F_RDLCK);
}

_PUBLIC_ void ntdb_chainunlock_read(struct ntdb_context *ntdb, NTDB_DATA key)
{
	uint32_t h = ntdb_hash(ntdb, key.dptr, key.dsize);

	ntdb_unlock_hash(ntdb, bits_from(h, 0, ntdb->hash_bits), F_RDLCK);
}
