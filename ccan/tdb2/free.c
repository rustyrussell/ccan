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
#include <time.h>
#include <assert.h>
#include <limits.h>

/* We have to be able to fit a free record here. */
#define MIN_DATA_LEN	\
	(sizeof(struct tdb_free_record) - sizeof(struct tdb_used_record))

/* We have a series of free lists, each one covering a "zone" of the file.
 *
 * For each zone we have a series of per-size buckets, and a final bucket for
 * "too big".
 *
 * It's possible to move the free_list_head, but *only* under the allrecord
 * lock. */
static tdb_off_t free_list_off(struct tdb_context *tdb, unsigned int list)
{
	return tdb->header.v.free_off + list * sizeof(tdb_off_t);
}

/* We're a library: playing with srandom() is unfriendly.  srandom_r
 * probably lacks portability.  We don't need very random here. */
static unsigned int quick_random(struct tdb_context *tdb)
{
	return getpid() + time(NULL) + (unsigned long)tdb;
}

/* Start by using a random zone to spread the load. */
void tdb_zone_init(struct tdb_context *tdb)
{
	/*
	 * We read num_zones without a proper lock, so we could have
	 * gotten a partial read.  Since zone_bits is 1 byte long, we
	 * can trust that; even if it's increased, the number of zones
	 * cannot have decreased.  And using the map size means we
	 * will not start with a zone which hasn't been filled yet.
	 */
	tdb->last_zone = quick_random(tdb)
		% ((tdb->map_size >> tdb->header.v.zone_bits) + 1);
}

static unsigned fls64(uint64_t val)
{
#if HAVE_BUILTIN_CLZL
	if (val <= ULONG_MAX) {
		/* This is significantly faster! */
		return val ? sizeof(long) * CHAR_BIT - __builtin_clzl(val) : 0;
	} else {
#endif
	uint64_t r = 64;

	if (!val)
		return 0;
	if (!(val & 0xffffffff00000000ull)) {
		val <<= 32;
		r -= 32;
	}
	if (!(val & 0xffff000000000000ull)) {
		val <<= 16;
		r -= 16;
	}
	if (!(val & 0xff00000000000000ull)) {
		val <<= 8;
		r -= 8;
	}
	if (!(val & 0xf000000000000000ull)) {
		val <<= 4;
		r -= 4;
	}
	if (!(val & 0xc000000000000000ull)) {
		val <<= 2;
		r -= 2;
	}
	if (!(val & 0x8000000000000000ull)) {
		val <<= 1;
		r -= 1;
	}
	return r;
#if HAVE_BUILTIN_CLZL
	}
#endif
}

/* In which bucket would we find a particular record size? (ignoring header) */
unsigned int size_to_bucket(struct tdb_context *tdb, tdb_len_t data_len)
{
	unsigned int bucket;

	/* We can't have records smaller than this. */
	assert(data_len >= MIN_DATA_LEN);

	/* Ignoring the header... */
	if (data_len - MIN_DATA_LEN <= 64) {
		/* 0 in bucket 0, 8 in bucket 1... 64 in bucket 6. */
		bucket = (data_len - MIN_DATA_LEN) / 8;
	} else {
		/* After that we go power of 2. */
		bucket = fls64(data_len - MIN_DATA_LEN) + 2;
	}

	if (unlikely(bucket > tdb->header.v.free_buckets))
		bucket = tdb->header.v.free_buckets;
	return bucket;
}

/* What zone does a block belong in? */ 
tdb_off_t zone_of(struct tdb_context *tdb, tdb_off_t off)
{
	assert(tdb->header_uptodate);

	return off >> tdb->header.v.zone_bits;
}

/* Returns free_buckets + 1, or list number to search. */
static tdb_off_t find_free_head(struct tdb_context *tdb, tdb_off_t bucket)
{
	tdb_off_t first, off;

	/* Speculatively search for a non-zero bucket. */
	first = tdb->last_zone * (tdb->header.v.free_buckets+1) + bucket;
	off = tdb_find_nonzero_off(tdb, free_list_off(tdb, first),
				   tdb->header.v.free_buckets + 1 - bucket);
	return bucket + off;
}

static int remove_from_list(struct tdb_context *tdb,
			    tdb_off_t list, struct tdb_free_record *r)
{
	tdb_off_t off;

	/* Front of list? */
	if (r->prev == 0) {
		off = free_list_off(tdb, list);
	} else {
		off = r->prev + offsetof(struct tdb_free_record, next);
	}
	/* r->prev->next = r->next */
	if (tdb_write_off(tdb, off, r->next)) {
		return -1;
	}

	if (r->next != 0) {
		off = r->next + offsetof(struct tdb_free_record, prev);
		/* r->next->prev = r->prev */
		if (tdb_write_off(tdb, off, r->prev)) {
			return -1;
		}
	}
	return 0;
}

/* Enqueue in this free list. */
static int enqueue_in_free(struct tdb_context *tdb,
			   tdb_off_t list,
			   tdb_off_t off,
			   struct tdb_free_record *new)
{
	new->prev = 0;
	/* new->next = head. */
	new->next = tdb_read_off(tdb, free_list_off(tdb, list));
	if (new->next == TDB_OFF_ERR)
		return -1;

	if (new->next) {
		/* next->prev = new. */
		if (tdb_write_off(tdb, new->next
				  + offsetof(struct tdb_free_record, prev),
				  off) != 0)
			return -1;
	}
	/* head = new */
	if (tdb_write_off(tdb, free_list_off(tdb, list), off) != 0)
		return -1;
	
	return tdb_write_convert(tdb, off, new, sizeof(*new));
}

/* List isn't locked. */
int add_free_record(struct tdb_context *tdb,
		    tdb_off_t off, tdb_len_t len_with_header)
{
	struct tdb_free_record new;
	tdb_off_t list;
	int ret;

	assert(len_with_header >= sizeof(new));

	new.magic = TDB_FREE_MAGIC;
	new.data_len = len_with_header - sizeof(struct tdb_used_record);

	tdb->last_zone = zone_of(tdb, off);
	list = tdb->last_zone * (tdb->header.v.free_buckets+1)
		+ size_to_bucket(tdb, new.data_len);

	if (tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) != 0)
		return -1;

	ret = enqueue_in_free(tdb, list, off, &new);
	tdb_unlock_free_list(tdb, list);
	return ret;
}

/* If we have enough left over to be useful, split that off. */
static int to_used_record(struct tdb_context *tdb,
			  tdb_off_t off,
			  tdb_len_t needed,
			  tdb_len_t total_len,
			  tdb_len_t *actual)
{
	struct tdb_used_record used;
	tdb_len_t leftover;

	leftover = total_len - needed;
	if (leftover < sizeof(struct tdb_free_record))
		leftover = 0;

	*actual = total_len - leftover;

	if (leftover) {
		if (add_free_record(tdb, off + sizeof(used) + *actual,
				    total_len - needed))
			return -1;
	}
	return 0;
}

/* Note: we unlock the current list if we coalesce or fail. */
static int coalesce(struct tdb_context *tdb, tdb_off_t off,
		    tdb_off_t list, tdb_len_t data_len)
{
	struct tdb_free_record pad, *r;
	tdb_off_t end = off + sizeof(struct tdb_used_record) + data_len;

	while (!tdb->methods->oob(tdb, end + sizeof(*r), 1)) {
		tdb_off_t nlist;

		r = tdb_get(tdb, end, &pad, sizeof(pad));
		if (!r)
			goto err;

		if (r->magic != TDB_FREE_MAGIC)
			break;

		nlist = zone_of(tdb, end) * (tdb->header.v.free_buckets+1)
			+ size_to_bucket(tdb, r->data_len);

		/* We may be violating lock order here, so best effort. */
		if (tdb_lock_free_list(tdb, nlist, TDB_LOCK_NOWAIT) == -1)
			break;

		/* Now we have lock, re-check. */
		r = tdb_get(tdb, end, &pad, sizeof(pad));
		if (!r) {
			tdb_unlock_free_list(tdb, nlist);
			goto err;
		}

		if (unlikely(r->magic != TDB_FREE_MAGIC)) {
			tdb_unlock_free_list(tdb, nlist);
			break;
		}

		if (remove_from_list(tdb, list, r) == -1) {
			tdb_unlock_free_list(tdb, nlist);
			goto err;
		}

		end += sizeof(struct tdb_used_record) + r->data_len;
		tdb_unlock_free_list(tdb, nlist);
	}

	/* Didn't find any adjacent free? */
	if (end == off + sizeof(struct tdb_used_record) + data_len)
		return 0;

	/* OK, expand record */
	r = tdb_get(tdb, off, &pad, sizeof(pad));
	if (!r)
		goto err;

	if (remove_from_list(tdb, list, r) == -1)
		goto err;

	/* We have to drop this to avoid deadlocks. */
	tdb_unlock_free_list(tdb, list);

	if (add_free_record(tdb, off, end - off) == -1)
		return -1;
	return 1;

err:
	/* To unify error paths, we *always* unlock list. */
	tdb_unlock_free_list(tdb, list);
	return -1;
}

/* We need size bytes to put our key and data in. */
static tdb_off_t lock_and_alloc(struct tdb_context *tdb,
				tdb_off_t bucket, size_t size,
				tdb_len_t *actual)
{
	tdb_off_t list;
	tdb_off_t off, best_off;
	struct tdb_free_record pad, best = { 0 }, *r;
	double multiplier;

again:
	list = tdb->last_zone * (tdb->header.v.free_buckets+1) + bucket;

	/* Lock this list. */
	if (tdb_lock_free_list(tdb, list, TDB_LOCK_WAIT) == -1) {
		return TDB_OFF_ERR;
	}

	best.data_len = -1ULL;
	best_off = 0;
	multiplier = 1.0;

	/* Walk the list to see if any are large enough, getting less fussy
	 * as we go. */
	off = tdb_read_off(tdb, free_list_off(tdb, list));
	if (unlikely(off == TDB_OFF_ERR))
		goto unlock_err;

	while (off) {
		r = tdb_get(tdb, off, &pad, sizeof(*r));
		if (!r)
			goto unlock_err;

		if (r->magic != TDB_FREE_MAGIC) {
			tdb->log(tdb, TDB_DEBUG_ERROR, tdb->log_priv,
				 "lock_and_alloc: %llu non-free 0x%llx\n",
				 (long long)off, (long long)r->magic);
			goto unlock_err;
		}

		if (r->data_len >= size && r->data_len < best.data_len) {
			best_off = off;
			best = *r;
		}

		if (best.data_len < size * multiplier && best_off)
			goto use_best;

		multiplier *= 1.01;

		/* Since we're going slow anyway, try coalescing here. */
		switch (coalesce(tdb, off, list, r->data_len)) {
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
	use_best:
		/* We're happy with this size: take it. */
		if (remove_from_list(tdb, list, &best) != 0)
			goto unlock_err;
		tdb_unlock_free_list(tdb, list);

		if (to_used_record(tdb, best_off, size, best.data_len,
				   actual)) {
			return -1;
		}
		return best_off;
	}

	tdb_unlock_free_list(tdb, list);
	return 0;

unlock_err:
	tdb_unlock_free_list(tdb, list);
	return TDB_OFF_ERR;
}

/* We want a really big chunk.  Look through every zone's oversize bucket */
static tdb_off_t huge_alloc(struct tdb_context *tdb, size_t size,
			    tdb_len_t *actual)
{
	tdb_off_t i, off;

	for (i = 0; i < tdb->header.v.num_zones; i++) {
		/* Try getting one from list. */
		off = lock_and_alloc(tdb, tdb->header.v.free_buckets,
				     size, actual);
		if (off == TDB_OFF_ERR)
			return TDB_OFF_ERR;
		if (off != 0)
			return off;
		/* FIXME: Coalesce! */
	}
	return 0;
}

static tdb_off_t get_free(struct tdb_context *tdb, size_t size,
			  tdb_len_t *actual)
{
	tdb_off_t off, bucket;
	unsigned int num_empty, step = 0;

	bucket = size_to_bucket(tdb, size);

	/* If we're after something bigger than a single zone, handle
	 * specially. */
	if (unlikely(sizeof(struct tdb_used_record) + size
		     >= (1ULL << tdb->header.v.zone_bits))) {
		return huge_alloc(tdb, size, actual);
	}

	/* Number of zones we search is proportional to the log of them. */
	for (num_empty = 0; num_empty < fls64(tdb->header.v.num_zones);
	     num_empty++) {
		tdb_off_t b;

		/* Start at exact size bucket, and search up... */
		for (b = bucket; b <= tdb->header.v.free_buckets; b++) {
			b = find_free_head(tdb, b);

			/* Non-empty list?  Try getting block. */
			if (b <= tdb->header.v.free_buckets) {
				/* Try getting one from list. */
				off = lock_and_alloc(tdb, b, size, actual);
				if (off == TDB_OFF_ERR)
					return TDB_OFF_ERR;
				if (off != 0)
					return off;
				/* Didn't work.  Try next bucket. */
			}
		}

		/* Try another zone, at pseudo random.  Avoid duplicates by
		   using an odd step. */
		if (step == 0)
			step = ((quick_random(tdb)) % 65536) * 2 + 1;
		tdb->last_zone = (tdb->last_zone + step)
			% tdb->header.v.num_zones;
	}
	return 0;
}

int set_header(struct tdb_context *tdb,
	       struct tdb_used_record *rec,
	       uint64_t keylen, uint64_t datalen,
	       uint64_t actuallen, uint64_t hash)
{
	uint64_t keybits = (fls64(keylen) + 1) / 2;

	/* Use top bits of hash, so it's independent of hash table size. */
	rec->magic_and_meta
		= (actuallen - (keylen + datalen))
		| ((hash >> 53) << 32)
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

static tdb_len_t adjust_size(size_t keylen, size_t datalen, bool growing)
{
	tdb_len_t size = keylen + datalen;

	if (size < MIN_DATA_LEN)
		size = MIN_DATA_LEN;

	/* Overallocate if this is coming from an enlarging store. */
	if (growing)
		size += datalen / 2;

	/* Round to next uint64_t boundary. */
	return (size + (sizeof(uint64_t) - 1ULL)) & ~(sizeof(uint64_t) - 1ULL);
}

/* If this fails, try tdb_expand. */
tdb_off_t alloc(struct tdb_context *tdb, size_t keylen, size_t datalen,
		uint64_t hash, bool growing)
{
	tdb_off_t off;
	tdb_len_t size, actual;
	struct tdb_used_record rec;

	/* We don't want header to change during this! */
	assert(tdb->header_uptodate);

	size = adjust_size(keylen, datalen, growing);

	off = get_free(tdb, size, &actual);
	if (unlikely(off == TDB_OFF_ERR || off == 0))
		return off;

	/* Some supergiant values can't be encoded. */
	if (set_header(tdb, &rec, keylen, datalen, actual, hash) != 0) {
		add_free_record(tdb, off, sizeof(rec) + actual);
		return TDB_OFF_ERR;
	}

	if (tdb_write_convert(tdb, off, &rec, sizeof(rec)) != 0)
		return TDB_OFF_ERR;
	
	return off;
}

static bool larger_buckets_might_help(struct tdb_context *tdb)
{
	/* If our buckets are already covering 1/8 of a zone, don't
	 * bother (note: might become an 1/16 of a zone if we double
	 * zone size). */
	tdb_len_t size = (1ULL << tdb->header.v.zone_bits) / 8;

	if (size >= MIN_DATA_LEN
	    && size_to_bucket(tdb, size) < tdb->header.v.free_buckets) {
		return false;
	}

	/* FIXME: Put stats in tdb_context or examine db itself! */
	/* It's fairly cheap to do as we expand database. */
	return true;
}

static bool zones_happy(struct tdb_context *tdb)
{
	/* FIXME: look at distribution of zones. */
	return true;
}

/* Expand the database. */
int tdb_expand(struct tdb_context *tdb, tdb_len_t klen, tdb_len_t dlen,
	       bool growing)
{
	uint64_t new_num_buckets, new_num_zones, new_zone_bits;
	uint64_t i, old_num_total, old_num_zones, old_size, old_zone_bits;
	tdb_len_t add, freebucket_size, needed;
	tdb_off_t off, old_free_off;
	const tdb_off_t *oldf;
	struct tdb_used_record fhdr;

	/* We need room for the record header too. */
	needed = sizeof(struct tdb_used_record)
		+ adjust_size(klen, dlen, growing);

	/* tdb_allrecord_lock will update header; did zones change? */
	old_zone_bits = tdb->header.v.zone_bits;
	old_num_zones = tdb->header.v.num_zones;

	/* FIXME: this is overkill.  An expand lock? */
	if (tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false) == -1)
		return -1;

	/* Someone may have expanded for us. */
	if (old_zone_bits != tdb->header.v.zone_bits
	    || old_num_zones != tdb->header.v.num_zones)
		goto success;

	/* They may have also expanded the underlying size (otherwise we'd
	 * have expanded our mmap to look at those offsets already). */
	old_size = tdb->map_size;
	tdb->methods->oob(tdb, tdb->map_size + 1, true);
	if (tdb->map_size != old_size)
		goto success;

	/* Did we enlarge zones without enlarging file? */
	if (tdb->map_size < tdb->header.v.num_zones<<tdb->header.v.zone_bits) {
		add = (tdb->header.v.num_zones<<tdb->header.v.zone_bits)
			- tdb->map_size;
		/* Updates tdb->map_size. */
		if (tdb->methods->expand_file(tdb, add) == -1)
			goto fail;
		if (add_free_record(tdb, tdb->map_size - add, add) == -1)
			goto fail;
		if (add >= needed) {
			/* Allocate from this zone. */
			tdb->last_zone = zone_of(tdb, tdb->map_size - add);
			goto success;
		}
	}

	/* Slow path.  Should we increase the number of buckets? */
	new_num_buckets = tdb->header.v.free_buckets;
	if (larger_buckets_might_help(tdb))
		new_num_buckets++;

	/* Now we'll need room for the new free buckets, too.  Assume
	 * worst case (zones expand). */
	needed += sizeof(fhdr)
		+ ((tdb->header.v.num_zones+1)
		   * (new_num_buckets+1) * sizeof(tdb_off_t));

	/* If we need less that one zone, and they're working well, just add
	 * another one. */
	if (needed < (1UL<<tdb->header.v.zone_bits) && zones_happy(tdb)) {
		new_num_zones = tdb->header.v.num_zones+1;
		new_zone_bits = tdb->header.v.zone_bits;
		add = 1ULL << tdb->header.v.zone_bits;
	} else {
		/* Increase the zone size. */
		new_num_zones = tdb->header.v.num_zones;
		new_zone_bits = tdb->header.v.zone_bits+1;
		while ((new_num_zones << new_zone_bits)
		       < tdb->map_size + needed) {
			new_zone_bits++;
		}

		/* We expand by enough full zones to meet the need. */
		add = ((tdb->map_size + needed + (1ULL << new_zone_bits)-1)
		       & ~((1ULL << new_zone_bits)-1))
			- tdb->map_size;
	}

	/* Updates tdb->map_size. */
	if (tdb->methods->expand_file(tdb, add) == -1)
		goto fail;

	/* Use first part as new free bucket array. */
	off = tdb->map_size - add;
	freebucket_size = new_num_zones
		* (new_num_buckets + 1) * sizeof(tdb_off_t);

	/* Write header. */
	if (set_header(tdb, &fhdr, 0, freebucket_size, freebucket_size, 0))
		goto fail;
	if (tdb_write_convert(tdb, off, &fhdr, sizeof(fhdr)) == -1)
		goto fail;

	/* Adjust off to point to start of buckets, add to be remainder. */
	add -= freebucket_size + sizeof(fhdr);
	off += sizeof(fhdr);

	/* Access the old zones. */
	old_num_total = tdb->header.v.num_zones*(tdb->header.v.free_buckets+1);
	old_free_off = tdb->header.v.free_off;
	oldf = tdb_access_read(tdb, old_free_off,
			       old_num_total * sizeof(tdb_off_t), true);
	if (!oldf)
		goto fail;

	/* Switch to using our new zone. */
	if (zero_out(tdb, off, freebucket_size) == -1)
		goto fail_release;

	tdb->header.v.free_off = off;
	tdb->header.v.num_zones = new_num_zones;
	tdb->header.v.zone_bits = new_zone_bits;
	tdb->header.v.free_buckets = new_num_buckets;

	/* FIXME: If zone size hasn't changed, can simply copy pointers. */
	/* FIXME: Coalesce? */
	for (i = 0; i < old_num_total; i++) {
		tdb_off_t next;
		struct tdb_free_record rec;
		tdb_off_t list;

		for (off = oldf[i]; off; off = next) {
			if (tdb_read_convert(tdb, off, &rec, sizeof(rec)))
				goto fail_release;

			list = zone_of(tdb, off)
				* (tdb->header.v.free_buckets+1)
				+ size_to_bucket(tdb, rec.data_len);
			next = rec.next;
		
			if (enqueue_in_free(tdb, list, off, &rec) == -1)
				goto fail_release;
		}
	}

	/* Free up the old free buckets. */
	old_free_off -= sizeof(fhdr);
	if (tdb_read_convert(tdb, old_free_off, &fhdr, sizeof(fhdr)) == -1)
		goto fail_release;
	if (add_free_record(tdb, old_free_off,
			    sizeof(fhdr)
			    + rec_data_length(&fhdr)
			    + rec_extra_padding(&fhdr)))
		goto fail_release;

	/* Add the rest as a new free record. */
	if (add_free_record(tdb, tdb->map_size - add, add) == -1)
		goto fail_release;

	/* Start allocating from where the new space is. */
	tdb->last_zone = zone_of(tdb, tdb->map_size - add);
	tdb_access_release(tdb, oldf);
	if (write_header(tdb) == -1)
		goto fail;
success:
	tdb_allrecord_unlock(tdb, F_WRLCK);
	return 0;

fail_release:
	tdb_access_release(tdb, oldf);
fail:
	tdb_allrecord_unlock(tdb, F_WRLCK);
	return -1;
}
