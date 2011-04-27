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

static enum TDB_ERROR check_header(struct tdb_context *tdb, tdb_off_t *recovery,
				   uint64_t *features)
{
	uint64_t hash_test;
	struct tdb_header hdr;
	enum TDB_ERROR ecode;

	ecode = tdb_read_convert(tdb, 0, &hdr, sizeof(hdr));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}
	/* magic food should not be converted, so convert back. */
	tdb_convert(tdb, hdr.magic_food, sizeof(hdr.magic_food));

	hash_test = TDB_HASH_MAGIC;
	hash_test = tdb_hash(tdb, &hash_test, sizeof(hash_test));
	if (hdr.hash_test != hash_test) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "check: hash test %llu should be %llu",
				  (long long)hdr.hash_test,
				  (long long)hash_test);
	}

	if (strcmp(hdr.magic_food, TDB_MAGIC_FOOD) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "check: bad magic '%.*s'",
				  (unsigned)sizeof(hdr.magic_food),
				  hdr.magic_food);
	}

	/* Features which are used must be a subset of features offered. */
	if (hdr.features_used & ~hdr.features_offered) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "check: features used (0x%llx) which"
				  " are not offered (0x%llx)",
				  (long long)hdr.features_used,
				  (long long)hdr.features_offered);
	}

	*features = hdr.features_offered;
	*recovery = hdr.recovery;
	if (*recovery) {
		if (*recovery < sizeof(hdr)
		    || *recovery > tdb->file->map_size) {
			return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					  "tdb_check:"
					  " invalid recovery offset %zu",
					  (size_t)*recovery);
		}
	}

	/* Don't check reserved: they *can* be used later. */
	return TDB_SUCCESS;
}

static enum TDB_ERROR check_hash_tree(struct tdb_context *tdb,
				      tdb_off_t off, unsigned int group_bits,
				      uint64_t hprefix,
				      unsigned hprefix_bits,
				      tdb_off_t used[],
				      size_t num_used,
				      size_t *num_found,
				      enum TDB_ERROR (*check)(TDB_DATA,
							      TDB_DATA, void *),
				      void *data);

static enum TDB_ERROR check_hash_chain(struct tdb_context *tdb,
				       tdb_off_t off,
				       uint64_t hash,
				       tdb_off_t used[],
				       size_t num_used,
				       size_t *num_found,
				       enum TDB_ERROR (*check)(TDB_DATA,
							       TDB_DATA,
							       void *),
				       void *data)
{
	struct tdb_used_record rec;
	enum TDB_ERROR ecode;

	ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (rec_magic(&rec) != TDB_CHAIN_MAGIC) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash chain magic %llu",
				  (long long)rec_magic(&rec));
	}

	if (rec_data_length(&rec) != sizeof(struct tdb_chain)) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check:"
				  " Bad hash chain length %llu vs %zu",
				  (long long)rec_data_length(&rec),
				  sizeof(struct tdb_chain));
	}
	if (rec_key_length(&rec) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash chain key length %llu",
				  (long long)rec_key_length(&rec));
	}
	if (rec_hash(&rec) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash chain hash value %llu",
				  (long long)rec_hash(&rec));
	}

	off += sizeof(rec);
	ecode = check_hash_tree(tdb, off, 0, hash, 64,
				used, num_used, num_found, check, data);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	off = tdb_read_off(tdb, off + offsetof(struct tdb_chain, next));
	if (TDB_OFF_IS_ERR(off)) {
		return off;
	}
	if (off == 0)
		return TDB_SUCCESS;
	(*num_found)++;
	return check_hash_chain(tdb, off, hash, used, num_used, num_found,
				check, data);
}

static enum TDB_ERROR check_hash_record(struct tdb_context *tdb,
					tdb_off_t off,
					uint64_t hprefix,
					unsigned hprefix_bits,
					tdb_off_t used[],
					size_t num_used,
					size_t *num_found,
					enum TDB_ERROR (*check)(TDB_DATA,
								TDB_DATA,
								void *),
					void *data)
{
	struct tdb_used_record rec;
	enum TDB_ERROR ecode;

	if (hprefix_bits >= 64)
		return check_hash_chain(tdb, off, hprefix, used, num_used,
					num_found, check, data);

	ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (rec_magic(&rec) != TDB_HTABLE_MAGIC) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash table magic %llu",
				  (long long)rec_magic(&rec));
	}
	if (rec_data_length(&rec)
	    != sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check:"
				  " Bad hash table length %llu vs %llu",
				  (long long)rec_data_length(&rec),
				  (long long)sizeof(tdb_off_t)
				  << TDB_SUBLEVEL_HASH_BITS);
	}
	if (rec_key_length(&rec) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash table key length %llu",
				  (long long)rec_key_length(&rec));
	}
	if (rec_hash(&rec) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Bad hash table hash value %llu",
				  (long long)rec_hash(&rec));
	}

	off += sizeof(rec);
	return check_hash_tree(tdb, off,
			       TDB_SUBLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS,
			       hprefix, hprefix_bits,
			       used, num_used, num_found, check, data);
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

static enum TDB_ERROR check_hash_tree(struct tdb_context *tdb,
				      tdb_off_t off, unsigned int group_bits,
				      uint64_t hprefix,
				      unsigned hprefix_bits,
				      tdb_off_t used[],
				      size_t num_used,
				      size_t *num_found,
				      enum TDB_ERROR (*check)(TDB_DATA,
							      TDB_DATA, void *),
				      void *data)
{
	unsigned int g, b;
	const tdb_off_t *hash;
	struct tdb_used_record rec;
	enum TDB_ERROR ecode;

	hash = tdb_access_read(tdb, off,
			       sizeof(tdb_off_t)
			       << (group_bits + TDB_HASH_GROUP_BITS),
			       true);
	if (TDB_PTR_IS_ERR(hash)) {
		return TDB_PTR_ERR(hash);
	}

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
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "tdb_check: Invalid offset"
						   " %llu in hash",
						   (long long)off);
				goto fail;
			}
			/* Mark it invalid. */
			*p ^= 1;
			(*num_found)++;

			if (hprefix_bits == 64) {
				/* Chained entries are unordered. */
				if (is_subhash(group[b])) {
					ecode = TDB_ERR_CORRUPT;
					tdb_logerr(tdb, ecode,
						   TDB_LOG_ERROR,
						   "tdb_check: Invalid chain"
						   " entry subhash");
					goto fail;
				}
				h = hash_record(tdb, off);
				if (h != hprefix) {
					ecode = TDB_ERR_CORRUPT;
					tdb_logerr(tdb, ecode,
						   TDB_LOG_ERROR,
						   "check: bad hash chain"
						   " placement"
						   " 0x%llx vs 0x%llx",
						   (long long)h,
						   (long long)hprefix);
					goto fail;
				}
				ecode = tdb_read_convert(tdb, off, &rec,
							 sizeof(rec));
				if (ecode != TDB_SUCCESS) {
					goto fail;
				}
				goto check;
			}

			if (is_subhash(group[b])) {
				uint64_t subprefix;
				subprefix = (hprefix
				     << (group_bits + TDB_HASH_GROUP_BITS))
					+ g * (1 << TDB_HASH_GROUP_BITS) + b;

				ecode = check_hash_record(tdb,
					       group[b] & TDB_OFF_MASK,
					       subprefix,
					       hprefix_bits
						       + group_bits
						       + TDB_HASH_GROUP_BITS,
					       used, num_used, num_found,
					       check, data);
				if (ecode != TDB_SUCCESS) {
					goto fail;
				}
				continue;
			}
			/* A normal entry */

			/* Does it belong here at all? */
			h = hash_record(tdb, off);
			used_bits = 0;
			if (get_bits(h, hprefix_bits, &used_bits) != hprefix
			    && hprefix_bits) {
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "check: bad hash placement"
						   " 0x%llx vs 0x%llx",
						   (long long)h,
						   (long long)hprefix);
				goto fail;
			}

			/* Does it belong in this group? */
			if (get_bits(h, group_bits, &used_bits) != g) {
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "check: bad group %llu"
						   " vs %u",
						   (long long)h, g);
				goto fail;
			}

			/* Are bucket bits correct? */
			bucket = group[b] & TDB_OFF_HASH_GROUP_MASK;
			if (get_bits(h, TDB_HASH_GROUP_BITS, &used_bits)
			    != bucket) {
				used_bits -= TDB_HASH_GROUP_BITS;
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "check: bad bucket %u vs %u",
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
					ecode = TDB_ERR_CORRUPT;
					tdb_logerr(tdb, ecode,
						   TDB_LOG_ERROR,
						   "check: bad group placement"
						   " %u vs %u",
						   b, bucket);
					goto fail;
				}
			}

			ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec));
			if (ecode != TDB_SUCCESS) {
				goto fail;
			}

			/* Bottom bits must match header. */
			if ((h & ((1 << 11)-1)) != rec_hash(&rec)) {
				ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT,
						   TDB_LOG_ERROR,
						   "tdb_check: Bad hash magic"
						   " at offset %llu"
						   " (0x%llx vs 0x%llx)",
						   (long long)off,
						   (long long)h,
						   (long long)rec_hash(&rec));
				goto fail;
			}

		check:
			if (check) {
				TDB_DATA k, d;
				const unsigned char *kptr;

				kptr = tdb_access_read(tdb,
						       off + sizeof(rec),
						       rec_key_length(&rec)
						       + rec_data_length(&rec),
						       false);
				if (TDB_PTR_IS_ERR(kptr)) {
					ecode = TDB_PTR_ERR(kptr);
					goto fail;
				}

				k = tdb_mkdata(kptr, rec_key_length(&rec));
				d = tdb_mkdata(kptr + k.dsize,
					       rec_data_length(&rec));
				ecode = check(k, d, data);
				tdb_access_release(tdb, kptr);
				if (ecode != TDB_SUCCESS) {
					goto fail;
				}
			}
		}
	}
	tdb_access_release(tdb, hash);
	return TDB_SUCCESS;

fail:
	tdb_access_release(tdb, hash);
	return ecode;
}

static enum TDB_ERROR check_hash(struct tdb_context *tdb,
				 tdb_off_t used[],
				 size_t num_used, size_t num_ftables,
				 int (*check)(TDB_DATA, TDB_DATA, void *),
				 void *data)
{
	/* Free tables also show up as used. */
	size_t num_found = num_ftables;
	enum TDB_ERROR ecode;

	ecode = check_hash_tree(tdb, offsetof(struct tdb_header, hashtable),
				TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS,
				0, 0, used, num_used, &num_found,
				check, data);
	if (ecode == TDB_SUCCESS) {
		if (num_found != num_used) {
			ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					   "tdb_check: Not all entries"
					   " are in hash");
		}
	}
	return ecode;
}

static enum TDB_ERROR check_free(struct tdb_context *tdb,
				 tdb_off_t off,
				 const struct tdb_free_record *frec,
				 tdb_off_t prev, unsigned int ftable,
				 unsigned int bucket)
{
	enum TDB_ERROR ecode;

	if (frec_magic(frec) != TDB_FREE_MAGIC) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: offset %llu bad magic 0x%llx",
				  (long long)off,
				  (long long)frec->magic_and_prev);
	}
	if (frec_ftable(frec) != ftable) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: offset %llu bad freetable %u",
				  (long long)off, frec_ftable(frec));

	}

	ecode = tdb->methods->oob(tdb, off
				  + frec_len(frec)
				  + sizeof(struct tdb_used_record),
				  false);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}
	if (size_to_bucket(frec_len(frec)) != bucket) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: offset %llu in wrong bucket"
				  " (%u vs %u)",
				  (long long)off,
				  bucket, size_to_bucket(frec_len(frec)));
	}
	if (prev && prev != frec_prev(frec)) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: offset %llu bad prev"
				  " (%llu vs %llu)",
				  (long long)off,
				  (long long)prev, (long long)frec_len(frec));
	}
	return TDB_SUCCESS;
}

static enum TDB_ERROR check_free_table(struct tdb_context *tdb,
				       tdb_off_t ftable_off,
				       unsigned ftable_num,
				       tdb_off_t fr[],
				       size_t num_free,
				       size_t *num_found)
{
	struct tdb_freetable ft;
	tdb_off_t h;
	unsigned int i;
	enum TDB_ERROR ecode;

	ecode = tdb_read_convert(tdb, ftable_off, &ft, sizeof(ft));
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	if (rec_magic(&ft.hdr) != TDB_FTABLE_MAGIC
	    || rec_key_length(&ft.hdr) != 0
	    || rec_data_length(&ft.hdr) != sizeof(ft) - sizeof(ft.hdr)
	    || rec_hash(&ft.hdr) != 0) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: Invalid header on free table");
	}

	for (i = 0; i < TDB_FREE_BUCKETS; i++) {
		tdb_off_t off, prev = 0, *p, first = 0;
		struct tdb_free_record f;

		h = bucket_off(ftable_off, i);
		for (off = tdb_read_off(tdb, h); off; off = f.next) {
			if (TDB_OFF_IS_ERR(off)) {
				return off;
			}
			if (!first) {
				off &= TDB_OFF_MASK;
				first = off;
			}
			ecode = tdb_read_convert(tdb, off, &f, sizeof(f));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
			ecode = check_free(tdb, off, &f, prev, ftable_num, i);
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}

			/* FIXME: Check hash bits */
			p = asearch(&off, fr, num_free, off_cmp);
			if (!p) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: Invalid offset"
						  " %llu in free table",
						  (long long)off);
			}
			/* Mark it invalid. */
			*p ^= 1;
			(*num_found)++;
			prev = off;
		}

		if (first) {
			/* Now we can check first back pointer. */
			ecode = tdb_read_convert(tdb, first, &f, sizeof(f));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
			ecode = check_free(tdb, first, &f, prev, ftable_num, i);
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
		}
	}
	return TDB_SUCCESS;
}

/* Slow, but should be very rare. */
tdb_off_t dead_space(struct tdb_context *tdb, tdb_off_t off)
{
	size_t len;
	enum TDB_ERROR ecode;

	for (len = 0; off + len < tdb->file->map_size; len++) {
		char c;
		ecode = tdb->methods->tread(tdb, off, &c, 1);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
		if (c != 0 && c != 0x43)
			break;
	}
	return len;
}

static enum TDB_ERROR check_linear(struct tdb_context *tdb,
				   tdb_off_t **used, size_t *num_used,
				   tdb_off_t **fr, size_t *num_free,
				   uint64_t features, tdb_off_t recovery)
{
	tdb_off_t off;
	tdb_len_t len;
	enum TDB_ERROR ecode;
	bool found_recovery = false;

	for (off = sizeof(struct tdb_header);
	     off < tdb->file->map_size;
	     off += len) {
		union {
			struct tdb_used_record u;
			struct tdb_free_record f;
			struct tdb_recovery_record r;
		} rec;
		/* r is larger: only get that if we need to. */
		ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec.f));
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}

		/* If we crash after ftruncate, we can get zeroes or fill. */
		if (rec.r.magic == TDB_RECOVERY_INVALID_MAGIC
		    || rec.r.magic ==  0x4343434343434343ULL) {
			ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec.r));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
			if (recovery == off) {
				found_recovery = true;
				len = sizeof(rec.r) + rec.r.max_len;
			} else {
				len = dead_space(tdb, off);
				if (TDB_OFF_IS_ERR(len)) {
					return len;
				}
				if (len < sizeof(rec.r)) {
					return tdb_logerr(tdb, TDB_ERR_CORRUPT,
							  TDB_LOG_ERROR,
							  "tdb_check: invalid"
							  " dead space at %zu",
							  (size_t)off);
				}

				tdb_logerr(tdb, TDB_SUCCESS, TDB_LOG_WARNING,
					   "Dead space at %zu-%zu (of %zu)",
					   (size_t)off, (size_t)(off + len),
					   (size_t)tdb->file->map_size);
			}
		} else if (rec.r.magic == TDB_RECOVERY_MAGIC) {
			ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec.r));
			if (ecode != TDB_SUCCESS) {
				return ecode;
			}
			if (recovery != off) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: unexpected"
						  " recovery record at offset"
						  " %zu",
						  (size_t)off);
			}
			if (rec.r.len > rec.r.max_len) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: invalid recovery"
						  " length %zu",
						  (size_t)rec.r.len);
			}
			if (rec.r.eof > tdb->file->map_size) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: invalid old EOF"
						  " %zu", (size_t)rec.r.eof);
			}
			found_recovery = true;
			len = sizeof(rec.r) + rec.r.max_len;
		} else if (frec_magic(&rec.f) == TDB_FREE_MAGIC) {
			len = sizeof(rec.u) + frec_len(&rec.f);
			if (off + len > tdb->file->map_size) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: free overlength"
						  " %llu at offset %llu",
						  (long long)len,
						  (long long)off);
			}
			/* This record should be in free lists. */
			if (frec_ftable(&rec.f) != TDB_FTABLE_NONE
			    && !append(fr, num_free, off)) {
				return tdb_logerr(tdb, TDB_ERR_OOM,
						  TDB_LOG_ERROR,
						  "tdb_check: tracking %zu'th"
						  " free record.", *num_free);
			}
		} else if (rec_magic(&rec.u) == TDB_USED_MAGIC
			   || rec_magic(&rec.u) == TDB_CHAIN_MAGIC
			   || rec_magic(&rec.u) == TDB_HTABLE_MAGIC
			   || rec_magic(&rec.u) == TDB_FTABLE_MAGIC) {
			uint64_t klen, dlen, extra;

			/* This record is used! */
			if (!append(used, num_used, off)) {
				return tdb_logerr(tdb, TDB_ERR_OOM,
						  TDB_LOG_ERROR,
						  "tdb_check: tracking %zu'th"
						  " used record.", *num_used);
			}

			klen = rec_key_length(&rec.u);
			dlen = rec_data_length(&rec.u);
			extra = rec_extra_padding(&rec.u);

			len = sizeof(rec.u) + klen + dlen + extra;
			if (off + len > tdb->file->map_size) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: used overlength"
						  " %llu at offset %llu",
						  (long long)len,
						  (long long)off);
			}

			if (len < sizeof(rec.f)) {
				return tdb_logerr(tdb, TDB_ERR_CORRUPT,
						  TDB_LOG_ERROR,
						  "tdb_check: too short record"
						  " %llu at %llu",
						  (long long)len,
						  (long long)off);
			}

			/* Check that records have correct 0 at end (but may
			 * not in future). */
			if (extra && !features) {
				const char *p;
				char c;
				p = tdb_access_read(tdb, off + sizeof(rec.u)
						    + klen + dlen, 1, false);
				if (TDB_PTR_IS_ERR(p))
					return TDB_PTR_ERR(p);
				c = *p;
				tdb_access_release(tdb, p);

				if (c != '\0') {
					return tdb_logerr(tdb, TDB_ERR_CORRUPT,
							  TDB_LOG_ERROR,
							  "tdb_check:"
							  " non-zero extra"
							  " at %llu",
							  (long long)off);
				}
			}
		} else {
			return tdb_logerr(tdb, TDB_ERR_CORRUPT,
					  TDB_LOG_ERROR,
					  "tdb_check: Bad magic 0x%llx"
					  " at offset %zu",
					  (long long)rec_magic(&rec.u),
					  (size_t)off);
		}
	}

	/* We must have found recovery area if there was one. */
	if (recovery != 0 && !found_recovery) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "tdb_check: expected a recovery area at %zu",
				  (size_t)recovery);
	}

	return TDB_SUCCESS;
}

enum TDB_ERROR tdb_check_(struct tdb_context *tdb,
			  enum TDB_ERROR (*check)(TDB_DATA, TDB_DATA, void *),
			  void *data)
{
	tdb_off_t *fr = NULL, *used = NULL, ft, recovery;
	size_t num_free = 0, num_used = 0, num_found = 0, num_ftables = 0;
	uint64_t features;
	enum TDB_ERROR ecode;

	ecode = tdb_allrecord_lock(tdb, F_RDLCK, TDB_LOCK_WAIT, false);
	if (ecode != TDB_SUCCESS) {
		return tdb->last_error = ecode;
	}

	ecode = tdb_lock_expand(tdb, F_RDLCK);
	if (ecode != TDB_SUCCESS) {
		tdb_allrecord_unlock(tdb, F_RDLCK);
		return tdb->last_error = ecode;
	}

	ecode = check_header(tdb, &recovery, &features);
	if (ecode != TDB_SUCCESS)
		goto out;

	/* First we do a linear scan, checking all records. */
	ecode = check_linear(tdb, &used, &num_used, &fr, &num_free, features,
			     recovery);
	if (ecode != TDB_SUCCESS)
		goto out;

	for (ft = first_ftable(tdb); ft; ft = next_ftable(tdb, ft)) {
		if (TDB_OFF_IS_ERR(ft)) {
			ecode = ft;
			goto out;
		}
		ecode = check_free_table(tdb, ft, num_ftables, fr, num_free,
					 &num_found);
		if (ecode != TDB_SUCCESS)
			goto out;
		num_ftables++;
	}

	/* FIXME: Check key uniqueness? */
	ecode = check_hash(tdb, used, num_used, num_ftables, check, data);
	if (ecode != TDB_SUCCESS)
		goto out;

	if (num_found != num_free) {
		ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				   "tdb_check: Not all entries are in"
				   " free table");
	}

out:
	tdb_allrecord_unlock(tdb, F_RDLCK);
	tdb_unlock_expand(tdb, F_RDLCK);
	free(fr);
	free(used);
	return tdb->last_error = ecode;
}
