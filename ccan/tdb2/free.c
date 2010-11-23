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

/* In which bucket would we find a particular record size? (ignoring header) */
unsigned int size_to_bucket(tdb_len_t data_len)
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

	if (unlikely(bucket >= TDB_FREE_BUCKETS))
		bucket = TDB_FREE_BUCKETS - 1;
	return bucket;
}

tdb_off_t first_flist(struct tdb_context *tdb)
{
	return tdb_read_off(tdb, offsetof(struct tdb_header, free_list));
}

tdb_off_t next_flist(struct tdb_context *tdb, tdb_off_t flist)
{
	return tdb_read_off(tdb, flist + offsetof(struct tdb_freelist, next));
}

int tdb_flist_init(struct tdb_context *tdb)
{
	/* Use reservoir sampling algorithm to select a free list at random. */
	unsigned int rnd, max = 0;
	tdb_off_t off;

	tdb->flist_off = off = first_flist(tdb);

	while (off) {
		if (off == TDB_OFF_ERR)
			return -1;

		rnd = random();
		if (rnd >= max) {
			tdb->flist_off = off;
			max = rnd;
		}

		off = next_flist(tdb, off);
	}
	return 0;
}

/* Offset of a given bucket. */
tdb_off_t bucket_off(tdb_off_t flist_off, unsigned bucket)
{
	return flist_off + offsetof(struct tdb_freelist, buckets)
		+ bucket * sizeof(tdb_off_t);
}

/* Returns free_buckets + 1, or list number to search. */
static tdb_off_t find_free_head(struct tdb_context *tdb,
				tdb_off_t flist_off,
				tdb_off_t bucket)
{
	/* Speculatively search for a non-zero bucket. */
	return tdb_find_nonzero_off(tdb, bucket_off(flist_off, 0),
				    bucket, TDB_FREE_BUCKETS);
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
		    tdb_off_t off, tdb_len_t len_with_header)
{
	struct tdb_free_record new;
	tdb_off_t b_off;
	int ret;

	assert(len_with_header >= sizeof(new));

	new.magic_and_meta = TDB_FREE_MAGIC << (64 - TDB_OFF_UPPER_STEAL)
		| tdb->flist_off;
	new.data_len = len_with_header - sizeof(struct tdb_used_record);

	b_off = bucket_off(tdb->flist_off, size_to_bucket(new.data_len));
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
		    tdb_off_t off, tdb_off_t b_off, tdb_len_t data_len)
{
	struct tdb_free_record pad, *r;
	tdb_off_t end;

	end = off + sizeof(struct tdb_used_record) + data_len;

	while (end < tdb->map_size) {
		tdb_off_t nb_off;

		/* FIXME: do tdb_get here and below really win? */
		r = tdb_get(tdb, end, &pad, sizeof(pad));
		if (!r)
			goto err;

		if (frec_magic(r) != TDB_FREE_MAGIC)
			break;

		nb_off = bucket_off(frec_flist(r), size_to_bucket(r->data_len));

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

		if (unlikely(bucket_off(frec_flist(r),
					size_to_bucket(r->data_len))
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
	r->magic_and_meta = TDB_COALESCING_MAGIC << (64 - TDB_OFF_UPPER_STEAL);
	r->data_len = end - off - sizeof(struct tdb_used_record);
	if (tdb_access_commit(tdb, r) != 0)
		goto err;

	tdb_unlock_free_bucket(tdb, b_off);

	if (add_free_record(tdb, off, end - off) == -1)
		return -1;
	return 1;

err:
	/* To unify error paths, we *always* unlock bucket on error. */
	tdb_unlock_free_bucket(tdb, b_off);
	return -1;
}

/* We need size bytes to put our key and data in. */
static tdb_off_t lock_and_alloc(struct tdb_context *tdb,
				tdb_off_t flist_off,
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
	b_off = bucket_off(flist_off, bucket);

	/* FIXME: Try non-blocking wait first, to measure contention. */
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
		switch (coalesce(tdb, off, b_off, r->data_len)) {
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
			       hashlow) != 0)
			goto unlock_err;

		if (tdb_write_convert(tdb, best_off, &rec, sizeof(rec)) != 0)
			goto unlock_err;

		tdb_unlock_free_bucket(tdb, b_off);

		if (leftover) {
			if (add_free_record(tdb,
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

/* Get a free block from current free list, or 0 if none. */
static tdb_off_t get_free(struct tdb_context *tdb,
			  size_t keylen, size_t datalen, bool want_extra,
			  unsigned hashlow)
{
	tdb_off_t off, flist;
	unsigned start_b, b;
	bool wrapped = false;

	/* If they are growing, add 50% to get to higher bucket. */
	if (want_extra)
		start_b = size_to_bucket(adjust_size(keylen,
						     datalen + datalen / 2));
	else
		start_b = size_to_bucket(adjust_size(keylen, datalen));

	flist = tdb->flist_off;
	while (!wrapped || flist != tdb->flist_off) {
		/* Start at exact size bucket, and search up... */
		for (b = find_free_head(tdb, flist, start_b);
		     b < TDB_FREE_BUCKETS;
		     b = find_free_head(tdb, flist, b + 1)) {
			/* Try getting one from list. */
			off = lock_and_alloc(tdb, flist,
					     b, keylen, datalen, want_extra,
					     hashlow);
			if (off == TDB_OFF_ERR)
				return TDB_OFF_ERR;
			if (off != 0) {
				/* Worked?  Stay using this list. */
				tdb->flist_off = flist;
				return off;
			}
			/* Didn't work.  Try next bucket. */
		}

		/* Hmm, try next list. */
		flist = next_flist(tdb, flist);
		if (flist == 0) {
			wrapped = true;
			flist = first_flist(tdb);
		}
	}

	return 0;
}

int set_header(struct tdb_context *tdb,
	       struct tdb_used_record *rec,
	       uint64_t keylen, uint64_t datalen,
	       uint64_t actuallen, unsigned hashlow)
{
	uint64_t keybits = (fls64(keylen) + 1) / 2;

	/* Use bottom bits of hash, so it's independent of hash table size. */
	rec->magic_and_meta = (hashlow & ((1 << 11)-1))
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

/* Expand the database. */
static int tdb_expand(struct tdb_context *tdb, tdb_len_t size)
{
	uint64_t old_size;
	tdb_len_t wanted;

	/* We need room for the record header too. */
	wanted = sizeof(struct tdb_used_record) + size;

	/* Need to hold a hash lock to expand DB: transactions rely on it. */
	if (!(tdb->flags & TDB_NOLOCK)
	    && !tdb->allrecord_lock.count && !tdb_has_hash_locks(tdb)) {
		tdb->log(tdb, TDB_DEBUG_FATAL, tdb->log_priv,
			 "tdb_expand: must hold lock during expand\n");
		return -1;
	}

	/* Only one person can expand file at a time. */
	if (tdb_lock_expand(tdb, F_WRLCK) != 0)
		return -1;

	/* Someone else may have expanded the file, so retry. */
	old_size = tdb->map_size;
	tdb->methods->oob(tdb, tdb->map_size + 1, true);
	if (tdb->map_size != old_size) {
		tdb_unlock_expand(tdb, F_WRLCK);
		return 0;
	}

	if (tdb->methods->expand_file(tdb, wanted*TDB_EXTENSION_FACTOR) == -1) {
		tdb_unlock_expand(tdb, F_WRLCK);
		return -1;
	}

	/* We need to drop this lock before adding free record. */
	tdb_unlock_expand(tdb, F_WRLCK);

	return add_free_record(tdb, old_size, wanted * TDB_EXTENSION_FACTOR);
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
