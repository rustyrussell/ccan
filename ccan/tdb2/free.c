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
#include <ccan/ilog/ilog.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

static unsigned fls64(uint64_t val)
{
	return ilog64(val);
}

static unsigned ffs64(uint64_t val)
{
#if HAVE_BUILTIN_FFSLL
	return __builtin_ffsll(val);
#else
	unsigned r = 0;

	if (!val)
		return 0;

	if (!(val & 0xffffffff)) {
		val >>= 32;
		r += 32;
	}
	if (!(val & 0xffff)) {
		val >>= 16;
		r += 16;
	}
	if (!(val & 0xff)) {
		val >>= 8;
		r += 8;
	}
	if (!(val & 0xf)) {
		val >>= 4;
		r += 4;
	}
	if (!(val & 0x3)) {
		val >>= 2;
		r += 2;
	}
	if (!(val & 0x1)) {
		val >>= 1;
		r += 1;
	}
	return r;
#endif
}

/* In which bucket would we find a particular record size? (ignoring header) */
unsigned int size_to_bucket(unsigned int zone_bits, tdb_len_t data_len)
{
	unsigned int bucket;

	/* We can't have records smaller than this. */
	assert(data_len >= TDB_MIN_DATA_LEN);

	/* Ignoring the header... */
	if (data_len - TDB_MIN_DATA_LEN <= 64) {
		/* 0 in bucket 0, 8 in bucket 1... 64 in bucket 8. */
		bucket = (data_len - TDB_MIN_DATA_LEN) / 8;
	} else {
		/* After that we go power of 2. */
		bucket = fls64(data_len - TDB_MIN_DATA_LEN) + 2;
	}

	if (unlikely(bucket > BUCKETS_FOR_ZONE(zone_bits)))
		bucket = BUCKETS_FOR_ZONE(zone_bits);
	return bucket;
}

/* Binary search for the zone for this offset. */
static tdb_off_t off_to_zone(struct tdb_context *tdb, tdb_off_t off,
			     struct free_zone_header *zhdr)
{
	tdb_off_t start, end;

	start = sizeof(struct tdb_header);
	end = start + (1ULL << fls64(tdb->map_size - start));

	for (;;) {
		if (tdb_read_convert(tdb, start, zhdr, sizeof(*zhdr)) == -1)
			return TDB_OFF_ERR;

		/* Is it inside this zone? */
		if (off < start + (1ULL << zhdr->zone_bits))
			return start;

		/* In practice, start + end won't overflow. */
		if (off >= (start + end) / 2)
			start = (start + end) / 2;
		else
			end = (start + end) / 2;
	}
}

static tdb_off_t last_zone(struct tdb_context *tdb,
			   struct free_zone_header *zhdr)
{
	return off_to_zone(tdb, tdb->map_size - 1, zhdr);
}

int tdb_zone_init(struct tdb_context *tdb)
{
	unsigned int i;
	uint64_t randoff = 0;

	/* We start in a random zone, to spread the load. */
	for (i = 0; i < 64; i += fls64(RAND_MAX))
		randoff ^= ((uint64_t)random()) << i;
	randoff = sizeof(struct tdb_header)
		+ (randoff % (tdb->map_size - sizeof(struct tdb_header)));

	tdb->zone_off = off_to_zone(tdb, randoff, &tdb->zhdr);
	if (tdb->zone_off == TDB_OFF_ERR)
		return -1;
	return 0;
}

/* Where's the header, given a zone size of 1 << zone_bits? */
static tdb_off_t zone_off(tdb_off_t off, unsigned int zone_bits)
{
	off -= sizeof(struct tdb_header);
	return (off & ~((1ULL << zone_bits) - 1)) + sizeof(struct tdb_header);
}

/* Offset of a given bucket. */
/* FIXME: bucket can be "unsigned" everywhere, or even uint8/16. */
tdb_off_t bucket_off(tdb_off_t zone_off, tdb_off_t bucket)
{
	return zone_off
		+ sizeof(struct free_zone_header)
		+ bucket * sizeof(tdb_off_t);
}

/* Returns free_buckets + 1, or list number to search. */
static tdb_off_t find_free_head(struct tdb_context *tdb, tdb_off_t bucket)
{
	/* Speculatively search for a non-zero bucket. */
	return tdb_find_nonzero_off(tdb, bucket_off(tdb->zone_off, 0),
				    bucket,
				    BUCKETS_FOR_ZONE(tdb->zhdr.zone_bits) + 1);
}

/* Remove from free bucket. */
static int remove_from_list(struct tdb_context *tdb,
			    tdb_off_t b_off, tdb_off_t r_off,
			    struct tdb_free_record *r)
{
	tdb_off_t off;

	/* Front of list? */
	if (r->prev == 0) {
		off = b_off;
	} else {
		off = r->prev + offsetof(struct tdb_free_record, next);
	}

#ifdef DEBUG
	if (tdb_read_off(tdb, off) != r_off) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "remove_from_list: %llu bad prev in list %llu\n",
			 (long long)r_off, (long long)b_off);
		return -1;
	}
#endif

	/* r->prev->next = r->next */
	if (tdb_write_off(tdb, off, r->next)) {
		return -1;
	}

	if (r->next != 0) {
		off = r->next + offsetof(struct tdb_free_record, prev);
		/* r->next->prev = r->prev */

#ifdef DEBUG
		if (tdb_read_off(tdb, off) != r_off) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "remove_from_list: %llu bad list %llu\n",
				 (long long)r_off, (long long)b_off);
			return -1;
		}
#endif

		if (tdb_write_off(tdb, off, r->prev)) {
			return -1;
		}
	}
	return 0;
}

/* Enqueue in this free bucket. */
static int enqueue_in_free(struct tdb_context *tdb,
			   tdb_off_t b_off,
			   tdb_off_t off,
			   struct tdb_free_record *new)
{
	new->prev = 0;
	/* new->next = head. */
	new->next = tdb_read_off(tdb, b_off);
	if (new->next == TDB_OFF_ERR)
		return -1;

	if (new->next) {
#ifdef DEBUG
		if (tdb_read_off(tdb,
				 new->next
				 + offsetof(struct tdb_free_record, prev))
		    != 0) {
			tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
				 "enqueue_in_free: %llu bad head prev %llu\n",
				 (long long)new->next, (long long)b_off);
			return -1;
		}
#endif
		/* next->prev = new. */
		if (tdb_write_off(tdb, new->next
				  + offsetof(struct tdb_free_record, prev),
				  off) != 0)
			return -1;
	}
	/* head = new */
	if (tdb_write_off(tdb, b_off, off) != 0)
		return -1;

	return tdb_write_convert(tdb, off, new, sizeof(*new));
}

/* List need not be locked. */
int add_free_record(struct tdb_context *tdb,
		    unsigned int zone_bits,
		    tdb_off_t off, tdb_len_t len_with_header)
{
	struct tdb_free_record new;
	tdb_off_t b_off;
	int ret;

	assert(len_with_header >= sizeof(new));
	assert(zone_bits < 64);

	new.magic_and_meta = TDB_FREE_MAGIC | zone_bits;
	new.data_len = len_with_header - sizeof(struct tdb_used_record);

	b_off = bucket_off(zone_off(off, zone_bits),
			   size_to_bucket(zone_bits, new.data_len));
	if (tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) != 0)
		return -1;

	ret = enqueue_in_free(tdb, b_off, off, &new);
	tdb_unlock_free_bucket(tdb, b_off);
	return ret;
}

static size_t adjust_size(size_t keylen, size_t datalen)
{
	size_t size = keylen + datalen;

	if (size < TDB_MIN_DATA_LEN)
		size = TDB_MIN_DATA_LEN;

	/* Round to next uint64_t boundary. */
	return (size + (sizeof(uint64_t) - 1ULL)) & ~(sizeof(uint64_t) - 1ULL);
}

/* If we have enough left over to be useful, split that off. */
static size_t record_leftover(size_t keylen, size_t datalen,
			      bool want_extra, size_t total_len)
{
	ssize_t leftover;

	if (want_extra)
		datalen += datalen / 2;
	leftover = total_len - adjust_size(keylen, datalen);

	if (leftover < (ssize_t)sizeof(struct tdb_free_record))
		return 0;

	return leftover;
}

/* Note: we unlock the current bucket if we coalesce or fail. */
static int coalesce(struct tdb_context *tdb,
		    tdb_off_t zone_off, unsigned zone_bits,
		    tdb_off_t off, tdb_off_t b_off, tdb_len_t data_len)
{
	struct tdb_free_record pad, *r;
	tdb_off_t zone_end, end;

	end = off + sizeof(struct tdb_used_record) + data_len;
	zone_end = zone_off + (1ULL << zone_bits);

	if (tdb->methods->oob(tdb, zone_end, true))
		zone_end = tdb->map_size;

	while (end < zone_end) {
		tdb_off_t nb_off;

		/* FIXME: do tdb_get here and below really win? */
		r = tdb_get(tdb, end, &pad, sizeof(pad));
		if (!r)
			goto err;

		if (frec_magic(r) != TDB_FREE_MAGIC)
			break;

		nb_off = bucket_off(zone_off,
				    size_to_bucket(zone_bits, r->data_len));

		/* We may be violating lock order here, so best effort. */
		if (tdb_lock_free_bucket(tdb, nb_off, TDB_LOCK_NOWAIT) == -1)
			break;

		/* Now we have lock, re-check. */
		r = tdb_get(tdb, end, &pad, sizeof(pad));
		if (!r) {
			tdb_unlock_free_bucket(tdb, nb_off);
			goto err;
		}

		if (unlikely(frec_magic(r) != TDB_FREE_MAGIC)) {
			tdb_unlock_free_bucket(tdb, nb_off);
			break;
		}

		if (unlikely(bucket_off(zone_off,
					size_to_bucket(zone_bits, r->data_len))
			     != nb_off)) {
			tdb_unlock_free_bucket(tdb, nb_off);
			break;
		}

		if (remove_from_list(tdb, nb_off, end, r) == -1) {
			tdb_unlock_free_bucket(tdb, nb_off);
			goto err;
		}

		end += sizeof(struct tdb_used_record) + r->data_len;
		tdb_unlock_free_bucket(tdb, nb_off);
	}

	/* Didn't find any adjacent free? */
	if (end == off + sizeof(struct tdb_used_record) + data_len)
		return 0;

	/* OK, expand record */
	r = tdb_get(tdb, off, &pad, sizeof(pad));
	if (!r)
		goto err;

	if (r->data_len != data_len) {
		tdb->ecode = TDB_ERR_CORRUPT;
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "coalesce: expected data len %llu not %llu\n",
			 (long long)data_len, (long long)r->data_len);
		goto err;
	}

	if (remove_from_list(tdb, b_off, off, r) == -1)
		goto err;

	r = tdb_access_write(tdb, off, sizeof(*r), true);
	if (!r)
		goto err;

	/* We have to drop this to avoid deadlocks, so make sure record
	 * doesn't get coalesced by someone else! */
	r->magic_and_meta = TDB_COALESCING_MAGIC | zone_bits;
	r->data_len = end - off - sizeof(struct tdb_used_record);
	if (tdb_access_commit(tdb, r) != 0)
		goto err;

	tdb_unlock_free_bucket(tdb, b_off);

	if (add_free_record(tdb, zone_bits, off, end - off) == -1)
		return -1;
	return 1;

err:
	/* To unify error paths, we *always* unlock bucket on error. */
	tdb_unlock_free_bucket(tdb, b_off);
	return -1;
}

/* We need size bytes to put our key and data in. */
static tdb_off_t lock_and_alloc(struct tdb_context *tdb,
				tdb_off_t zone_off,
				unsigned zone_bits,
				tdb_off_t bucket,
				size_t keylen, size_t datalen,
				bool want_extra,
				unsigned hashlow)
{
	tdb_off_t off, b_off,best_off;
	struct tdb_free_record pad, best = { 0 }, *r;
	double multiplier;
	size_t size = adjust_size(keylen, datalen);

again:
	b_off = bucket_off(zone_off, bucket);

	/* FIXME: Try non-blocking wait first, to measure contention.
	 * If we're contented, try switching zones, and don't enlarge zone
	 * next time (we want more zones). */
	/* Lock this bucket. */
	if (tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT) == -1) {
		return TDB_OFF_ERR;
	}

	best.data_len = -1ULL;
	best_off = 0;

	/* Get slack if we're after extra. */
	if (want_extra)
		multiplier = 1.5;
	else
		multiplier = 1.0;

	/* Walk the list to see if any are large enough, getting less fussy
	 * as we go. */
	off = tdb_read_off(tdb, b_off);
	if (unlikely(off == TDB_OFF_ERR))
		goto unlock_err;

	while (off) {
		/* FIXME: Does tdb_get win anything here? */
		r = tdb_get(tdb, off, &pad, sizeof(*r));
		if (!r)
			goto unlock_err;

		if (frec_magic(r) != TDB_FREE_MAGIC) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "lock_and_alloc: %llu non-free 0x%llx\n",
				 (long long)off, (long long)r->magic_and_meta);
			goto unlock_err;
		}

		if (r->data_len >= size && r->data_len < best.data_len) {
			best_off = off;
			best = *r;
		}

		if (best.data_len < size * multiplier && best_off)
			break;

		multiplier *= 1.01;

		/* Since we're going slow anyway, try coalescing here. */
		switch (coalesce(tdb, zone_off, zone_bits, off, b_off,
				 r->data_len)) {
		case -1:
			/* This has already unlocked on error. */
			return -1;
		case 1:
			/* This has unlocked list, restart. */
			goto again;
		}
		off = r->next;
	}

	/* If we found anything at all, use it. */
	if (best_off) {
		struct tdb_used_record rec;
		size_t leftover;

		/* We're happy with this size: take it. */
		if (remove_from_list(tdb, b_off, best_off, &best) != 0)
			goto unlock_err;

		leftover = record_leftover(keylen, datalen, want_extra,
					   best.data_len);

		assert(keylen + datalen + leftover <= best.data_len);
		/* We need to mark non-free before we drop lock, otherwise
		 * coalesce() could try to merge it! */
		if (set_header(tdb, &rec, keylen, datalen,
			       best.data_len - leftover,
			       hashlow, zone_bits) != 0)
			goto unlock_err;

		if (tdb_write_convert(tdb, best_off, &rec, sizeof(rec)) != 0)
			goto unlock_err;

		tdb_unlock_free_bucket(tdb, b_off);

		if (leftover) {
			if (add_free_record(tdb, zone_bits,
					    best_off + sizeof(rec)
					    + best.data_len - leftover,
					    leftover))
				return TDB_OFF_ERR;
		}
		return best_off;
	}

	tdb_unlock_free_bucket(tdb, b_off);
	return 0;

unlock_err:
	tdb_unlock_free_bucket(tdb, b_off);
	return TDB_OFF_ERR;
}

static bool next_zone(struct tdb_context *tdb)
{
	tdb_off_t next = tdb->zone_off + (1ULL << tdb->zhdr.zone_bits);

	/* We must have a header. */
	if (tdb->methods->oob(tdb, next + sizeof(tdb->zhdr), true))
		return false;

	tdb->zone_off = next;
	return tdb_read_convert(tdb, next, &tdb->zhdr, sizeof(tdb->zhdr)) == 0;
}

/* Offset returned is within current zone (which it may alter). */
static tdb_off_t get_free(struct tdb_context *tdb,
			  size_t keylen, size_t datalen, bool want_extra,
			  unsigned hashlow)
{
	tdb_off_t start_zone = tdb->zone_off, off;
	bool wrapped = false;
	size_t size = adjust_size(keylen, datalen);

	/* If they are growing, add 50% to get to higher bucket. */
	if (want_extra)
		size += datalen / 2;

	/* FIXME: If we don't get a hit in the first bucket we want,
	 * try changing zones for next time.  That should help wear
	 * zones evenly, so we don't need to search all of them before
	 * expanding. */
	while (!wrapped || tdb->zone_off != start_zone) {
		tdb_off_t b;

		/* Shortcut for really huge allocations... */
		if ((size >> tdb->zhdr.zone_bits) != 0)
			goto next;

		/* Start at exact size bucket, and search up... */
		b = size_to_bucket(tdb->zhdr.zone_bits, size);
		for (b = find_free_head(tdb, b);
		     b <= BUCKETS_FOR_ZONE(tdb->zhdr.zone_bits);
		     b += find_free_head(tdb, b + 1)) {
			/* Try getting one from list. */
			off = lock_and_alloc(tdb, tdb->zone_off,
					     tdb->zhdr.zone_bits,
					     b, keylen, datalen, want_extra,
					     hashlow);
			if (off == TDB_OFF_ERR)
				return TDB_OFF_ERR;
			if (off != 0)
				return off;
			/* Didn't work.  Try next bucket. */
		}

	next:
		/* Didn't work, try next zone, if it exists. */
		if (!next_zone(tdb)) {
			wrapped = true;
			tdb->zone_off = sizeof(struct tdb_header);
			if (tdb_read_convert(tdb, tdb->zone_off,
					     &tdb->zhdr, sizeof(tdb->zhdr))) {
				return TDB_OFF_ERR;
			}
		}
	}
	return 0;
}

int set_header(struct tdb_context *tdb,
	       struct tdb_used_record *rec,
	       uint64_t keylen, uint64_t datalen,
	       uint64_t actuallen, unsigned hashlow,
	       unsigned int zone_bits)
{
	uint64_t keybits = (fls64(keylen) + 1) / 2;

	/* Use bottom bits of hash, so it's independent of hash table size. */
	rec->magic_and_meta
		= zone_bits
		| ((hashlow & ((1 << 5)-1)) << 6)
		| ((actuallen - (keylen + datalen)) << 11)
		| (keybits << 43)
		| (TDB_MAGIC << 48);
	rec->key_and_data_len = (keylen | (datalen << (keybits*2)));

	/* Encoding can fail on big values. */
	if (rec_key_length(rec) != keylen
	    || rec_data_length(rec) != datalen
	    || rec_extra_padding(rec) != actuallen - (keylen + datalen)) {
		tdb->ecode = TDB_ERR_IO;
		tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
			 "Could not encode k=%llu,d=%llu,a=%llu\n",
			 (long long)keylen, (long long)datalen,
			 (long long)actuallen);
		return -1;
	}
	return 0;
}

static bool zones_contended(struct tdb_context *tdb)
{
	return false;
}

/* Assume we want buckets up to the comfort factor. */
static tdb_len_t overhead(unsigned int zone_bits)
{
	return sizeof(struct free_zone_header)
		+ (BUCKETS_FOR_ZONE(zone_bits) + 1) * sizeof(tdb_off_t);
}

/* Expand the database (by adding a zone). */
static int tdb_expand(struct tdb_context *tdb, tdb_len_t size)
{
	uint64_t old_size;
	tdb_off_t off;
	unsigned int num_buckets, zone_bits;
	tdb_len_t wanted, expand;
	struct free_zone_header zhdr;

	/* We need room for the record header too. */
	wanted = sizeof(struct tdb_used_record) + size;

	/* Only one person can expand file at a time. */
	if (tdb_lock_expand(tdb, F_WRLCK) != 0)
		return -1;

	/* Someone else may have expanded the file, so retry. */
	old_size = tdb->map_size;
	tdb->methods->oob(tdb, tdb->map_size + 1, true);
	if (tdb->map_size != old_size)
		goto success;

	/* Treat last zone as minimum reasonable zone size. */
	off = last_zone(tdb, &zhdr);
	if (off == TDB_OFF_ERR)
		goto fail;

	/* Zone isn't fully expanded? */
	if (tdb->map_size < off + (1ULL << zhdr.zone_bits)) {
		expand = off + (1ULL << zhdr.zone_bits) - tdb->map_size;
		/* Expand more than we want. */
		if (expand > (wanted << TDB_COMFORT_FACTOR_BITS))
			expand = (wanted << TDB_COMFORT_FACTOR_BITS);
		if (tdb->methods->expand_file(tdb, expand) == -1)
			goto fail;
		/* We need to drop this lock before adding free record. */
		tdb_unlock_expand(tdb, F_WRLCK);

		/* Allocate from here. */
		tdb->zone_off = off;
		tdb->zhdr = zhdr;

		/* FIXME: If this isn't sufficient, we search again... */
		return add_free_record(tdb, zhdr.zone_bits,
				       tdb->map_size - expand, expand);
	}

	/* We are never allowed to cross a power-of-two boundary, and our
	 * minimum zone size is 1 << INITIAL_ZONE_BITS.
	 *
	 * If our filesize is 128k, we can add a 64k or a 128k zone.  If it's
	 * 192k, we can only add a 64k zone.
	 *
	 * In other words, our max zone size is (1 << (ffs(filesize) - 1)) */
	zone_bits = ffs64(old_size - sizeof(struct tdb_header)) - 1;
	assert(zone_bits >= INITIAL_ZONE_BITS);

	/* Big zones generally good, but more zones wanted if contended. */
	if (zones_contended(tdb)) {
		/* If it suffices, make zone same size as last one. */
		if (zhdr.zone_bits < zone_bits
		    && (1ULL << zhdr.zone_bits) >= overhead(zone_bits)+wanted)
			zone_bits = zhdr.zone_bits;
	}

	zhdr.zone_bits = zone_bits;
	num_buckets = BUCKETS_FOR_ZONE(zone_bits);

	/* Expand the file by more than we need right now. */
	expand = 1ULL << zone_bits;
	if (expand > overhead(zone_bits) + (wanted << TDB_COMFORT_FACTOR_BITS))
		expand = overhead(zone_bits)
			+ (wanted << TDB_COMFORT_FACTOR_BITS);

	if (tdb->methods->expand_file(tdb, expand) == -1)
		goto fail;

	/* Write new zone header (at old end). */
	off = old_size;
	if (tdb_write_convert(tdb, off, &zhdr, sizeof(zhdr)) == -1)
		goto fail;

	/* Now write empty buckets. */
	off += sizeof(zhdr);
	if (zero_out(tdb, off, (num_buckets+1) * sizeof(tdb_off_t)) == -1)
		goto fail;
	off += (num_buckets+1) * sizeof(tdb_off_t);

	/* Now add the rest as our free record. */
	if (add_free_record(tdb, zone_bits, off, expand - overhead(zone_bits))
	    == -1)
		goto fail;

	/* Try allocating from this zone now. */
	tdb->zone_off = old_size;
	tdb->zhdr = zhdr;

success:
	tdb_unlock_expand(tdb, F_WRLCK);
	return 0;

fail:
	tdb_unlock_expand(tdb, F_WRLCK);
	return -1;
}

/* This won't fail: it will expand the database if it has to. */
tdb_off_t alloc(struct tdb_context *tdb, size_t keylen, size_t datalen,
		uint64_t hash, bool growing)
{
	tdb_off_t off;

	/* We can't hold pointers during this: we could unmap! */
	assert(!tdb->direct_access);

	for (;;) {
		off = get_free(tdb, keylen, datalen, growing, hash);
		if (likely(off != 0))
			break;

		if (tdb_expand(tdb, adjust_size(keylen, datalen)))
			return TDB_OFF_ERR;
	}

	return off;
}
