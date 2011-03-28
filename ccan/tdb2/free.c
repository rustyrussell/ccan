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

tdb_off_t first_ftable(struct tdb_context *tdb)
{
	return tdb_read_off(tdb, offsetof(struct tdb_header, free_table));
}

tdb_off_t next_ftable(struct tdb_context *tdb, tdb_off_t ftable)
{
	return tdb_read_off(tdb, ftable + offsetof(struct tdb_freetable,next));
}

enum TDB_ERROR tdb_ftable_init(struct tdb_context *tdb)
{
	/* Use reservoir sampling algorithm to select a free list at random. */
	unsigned int rnd, max = 0, count = 0;
	tdb_off_t off;

	tdb->ftable_off = off = first_ftable(tdb);
	tdb->ftable = 0;

	while (off) {
		if (TDB_OFF_IS_ERR(off)) {
			return off;
		}

		rnd = random();
		if (rnd >= max) {
			tdb->ftable_off = off;
			tdb->ftable = count;
			max = rnd;
		}

		off = next_ftable(tdb, off);
		count++;
	}
	return TDB_SUCCESS;
}

/* Offset of a given bucket. */
tdb_off_t bucket_off(tdb_off_t ftable_off, unsigned bucket)
{
	return ftable_off + offsetof(struct tdb_freetable, buckets)
		+ bucket * sizeof(tdb_off_t);
}

/* Returns free_buckets + 1, or list number to search, or -ve error. */
static tdb_off_t find_free_head(struct tdb_context *tdb,
				tdb_off_t ftable_off,
				tdb_off_t bucket)
{
	/* Speculatively search for a non-zero bucket. */
	return tdb_find_nonzero_off(tdb, bucket_off(ftable_off, 0),
				    bucket, TDB_FREE_BUCKETS);
}

static void check_list(struct tdb_context *tdb, tdb_off_t b_off)
{
#ifdef CCAN_TDB2_DEBUG
	tdb_off_t off, prev = 0, first;
	struct tdb_free_record r;

	first = off = tdb_read_off(tdb, b_off);
	while (off != 0) {
		tdb_read_convert(tdb, off, &r, sizeof(r));
		if (frec_magic(&r) != TDB_FREE_MAGIC)
			abort();
		if (prev && frec_prev(&r) != prev)
			abort();
		prev = off;
		off = r.next;
	}

	if (first) {
		tdb_read_convert(tdb, first, &r, sizeof(r));
		if (frec_prev(&r) != prev)
			abort();
	}
#endif
}

/* Remove from free bucket. */
static enum TDB_ERROR remove_from_list(struct tdb_context *tdb,
				       tdb_off_t b_off, tdb_off_t r_off,
				       const struct tdb_free_record *r)
{
	tdb_off_t off, prev_next, head;
	enum TDB_ERROR ecode;

	/* Is this only element in list?  Zero out bucket, and we're done. */
	if (frec_prev(r) == r_off)
		return tdb_write_off(tdb, b_off, 0);

	/* off = &r->prev->next */
	off = frec_prev(r) + offsetof(struct tdb_free_record, next);

	/* Get prev->next */
	prev_next = tdb_read_off(tdb, off);
	if (TDB_OFF_IS_ERR(prev_next))
		return prev_next;

	/* If prev->next == 0, we were head: update bucket to point to next. */
	if (prev_next == 0) {
#ifdef CCAN_TDB2_DEBUG
		if (tdb_read_off(tdb, b_off) != r_off) {
			return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					  "remove_from_list:"
					  " %llu head %llu on list %llu",
					  (long long)r_off,
					  (long long)tdb_read_off(tdb, b_off),
					  (long long)b_off);
		}
#endif
		ecode = tdb_write_off(tdb, b_off, r->next);
		if (ecode != TDB_SUCCESS)
			return ecode;
	} else {
		/* r->prev->next = r->next */
		ecode = tdb_write_off(tdb, off, r->next);
		if (ecode != TDB_SUCCESS)
			return ecode;
	}

	/* If we were the tail, off = &head->prev. */
	if (r->next == 0) {
		head = tdb_read_off(tdb, b_off);
		if (TDB_OFF_IS_ERR(head))
			return head;
		off = head + offsetof(struct tdb_free_record, magic_and_prev);
	} else {
		/* off = &r->next->prev */
		off = r->next + offsetof(struct tdb_free_record,
					 magic_and_prev);
	}

#ifdef CCAN_TDB2_DEBUG
	/* *off == r */
	if ((tdb_read_off(tdb, off) & TDB_OFF_MASK) != r_off) {
		return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				  "remove_from_list:"
				  " %llu bad prev in list %llu",
				  (long long)r_off, (long long)b_off);
	}
#endif
	/* r->next->prev = r->prev */
	return tdb_write_off(tdb, off, r->magic_and_prev);
}

/* Enqueue in this free bucket. */
static enum TDB_ERROR enqueue_in_free(struct tdb_context *tdb,
				      tdb_off_t b_off,
				      tdb_off_t off,
				      tdb_len_t len)
{
	struct tdb_free_record new;
	enum TDB_ERROR ecode;
	tdb_off_t prev;
	uint64_t magic = (TDB_FREE_MAGIC << (64 - TDB_OFF_UPPER_STEAL));

	/* We only need to set ftable_and_len; rest is set in enqueue_in_free */
	new.ftable_and_len = ((uint64_t)tdb->ftable << (64 - TDB_OFF_UPPER_STEAL))
		| len;

	/* new->next = head. */
	new.next = tdb_read_off(tdb, b_off);
	if (TDB_OFF_IS_ERR(new.next)) {
		return new.next;
	}

	/* First element?  Prev points to ourselves. */
	if (!new.next) {
		new.magic_and_prev = (magic | off);
	} else {
		/* new->prev = next->prev */
		prev = tdb_read_off(tdb,
				    new.next + offsetof(struct tdb_free_record,
							magic_and_prev));
		new.magic_and_prev = prev;
		if (frec_magic(&new) != TDB_FREE_MAGIC) {
			return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					  "enqueue_in_free: %llu bad head"
					  " prev %llu",
					  (long long)new.next,
					  (long long)prev);
		}
		/* next->prev = new. */
		ecode = tdb_write_off(tdb, new.next
				      + offsetof(struct tdb_free_record,
						 magic_and_prev),
				      off | magic);
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}

#ifdef CCAN_TDB2_DEBUG
		prev = tdb_read_off(tdb, frec_prev(&new)
				    + offsetof(struct tdb_free_record, next));
		if (prev != 0) {
			return tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					  "enqueue_in_free:"
					  " %llu bad tail next ptr %llu",
					  (long long)frec_prev(&new)
					  + offsetof(struct tdb_free_record,
						     next),
					  (long long)prev);
		}
#endif
	}
	/* head = new */
	ecode = tdb_write_off(tdb, b_off, off);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	return tdb_write_convert(tdb, off, &new, sizeof(new));
}

/* List need not be locked. */
enum TDB_ERROR add_free_record(struct tdb_context *tdb,
			       tdb_off_t off, tdb_len_t len_with_header)
{
	tdb_off_t b_off;
	tdb_len_t len;
	enum TDB_ERROR ecode;

	assert(len_with_header >= sizeof(struct tdb_free_record));

	len = len_with_header - sizeof(struct tdb_used_record);

	b_off = bucket_off(tdb->ftable_off, size_to_bucket(len));
	ecode = tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	ecode = enqueue_in_free(tdb, b_off, off, len);
	check_list(tdb, b_off);
	tdb_unlock_free_bucket(tdb, b_off);
	return ecode;
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

static tdb_off_t ftable_offset(struct tdb_context *tdb, unsigned int ftable)
{
	tdb_off_t off;
	unsigned int i;

	if (likely(tdb->ftable == ftable))
		return tdb->ftable_off;

	off = first_ftable(tdb);
	for (i = 0; i < ftable; i++) {
		if (TDB_OFF_IS_ERR(off)) {
			break;
		}
		off = next_ftable(tdb, off);
	}
	return off;
}

/* Note: we unlock the current bucket if we coalesce or fail. */
static tdb_bool_err coalesce(struct tdb_context *tdb,
			     tdb_off_t off, tdb_off_t b_off,
			     tdb_len_t data_len)
{
	tdb_off_t end;
	struct tdb_free_record rec;
	enum TDB_ERROR ecode;

	add_stat(tdb, alloc_coalesce_tried, 1);
	end = off + sizeof(struct tdb_used_record) + data_len;

	while (end < tdb->file->map_size) {
		const struct tdb_free_record *r;
		tdb_off_t nb_off;
		unsigned ftable, bucket;

		r = tdb_access_read(tdb, end, sizeof(*r), true);
		if (TDB_PTR_IS_ERR(r)) {
			ecode = TDB_PTR_ERR(r);
			goto err;
		}

		if (frec_magic(r) != TDB_FREE_MAGIC
		    || frec_ftable(r) == TDB_FTABLE_NONE) {
			tdb_access_release(tdb, r);
			break;
		}

		ftable = frec_ftable(r);
		bucket = size_to_bucket(frec_len(r));
		nb_off = ftable_offset(tdb, ftable);
		if (TDB_OFF_IS_ERR(nb_off)) {
			tdb_access_release(tdb, r);
			ecode = nb_off;
			goto err;
		}
		nb_off = bucket_off(nb_off, bucket);
		tdb_access_release(tdb, r);

		/* We may be violating lock order here, so best effort. */
		if (tdb_lock_free_bucket(tdb, nb_off, TDB_LOCK_NOWAIT)
		    != TDB_SUCCESS) {
			add_stat(tdb, alloc_coalesce_lockfail, 1);
			break;
		}

		/* Now we have lock, re-check. */
		ecode = tdb_read_convert(tdb, end, &rec, sizeof(rec));
		if (ecode != TDB_SUCCESS) {
			tdb_unlock_free_bucket(tdb, nb_off);
			goto err;
		}

		if (unlikely(frec_magic(&rec) != TDB_FREE_MAGIC)) {
			add_stat(tdb, alloc_coalesce_race, 1);
			tdb_unlock_free_bucket(tdb, nb_off);
			break;
		}

		if (unlikely(frec_ftable(&rec) != ftable)
		    || unlikely(size_to_bucket(frec_len(&rec)) != bucket)) {
			add_stat(tdb, alloc_coalesce_race, 1);
			tdb_unlock_free_bucket(tdb, nb_off);
			break;
		}

		ecode = remove_from_list(tdb, nb_off, end, &rec);
		check_list(tdb, nb_off);
		if (ecode != TDB_SUCCESS) {
			tdb_unlock_free_bucket(tdb, nb_off);
			goto err;
		}

		end += sizeof(struct tdb_used_record) + frec_len(&rec);
		tdb_unlock_free_bucket(tdb, nb_off);
		add_stat(tdb, alloc_coalesce_num_merged, 1);
	}

	/* Didn't find any adjacent free? */
	if (end == off + sizeof(struct tdb_used_record) + data_len)
		return false;

	/* OK, expand initial record */
	ecode = tdb_read_convert(tdb, off, &rec, sizeof(rec));
	if (ecode != TDB_SUCCESS) {
		goto err;
	}

	if (frec_len(&rec) != data_len) {
		ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
				   "coalesce: expected data len %zu not %zu",
				   (size_t)data_len, (size_t)frec_len(&rec));
		goto err;
	}

	ecode = remove_from_list(tdb, b_off, off, &rec);
	check_list(tdb, b_off);
	if (ecode != TDB_SUCCESS) {
		goto err;
	}

	/* We have to drop this to avoid deadlocks, so make sure record
	 * doesn't get coalesced by someone else! */
	rec.ftable_and_len = (TDB_FTABLE_NONE << (64 - TDB_OFF_UPPER_STEAL))
		| (end - off - sizeof(struct tdb_used_record));
	ecode = tdb_write_off(tdb, off + offsetof(struct tdb_free_record,
						  ftable_and_len),
			      rec.ftable_and_len);
	if (ecode != TDB_SUCCESS) {
		goto err;
	}

	add_stat(tdb, alloc_coalesce_succeeded, 1);
	tdb_unlock_free_bucket(tdb, b_off);

	ecode = add_free_record(tdb, off, end - off);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}
	return true;

err:
	/* To unify error paths, we *always* unlock bucket on error. */
	tdb_unlock_free_bucket(tdb, b_off);
	return ecode;
}

/* We need size bytes to put our key and data in. */
static tdb_off_t lock_and_alloc(struct tdb_context *tdb,
				tdb_off_t ftable_off,
				tdb_off_t bucket,
				size_t keylen, size_t datalen,
				bool want_extra,
				unsigned magic,
				unsigned hashlow)
{
	tdb_off_t off, b_off,best_off;
	struct tdb_free_record best = { 0 };
	double multiplier;
	size_t size = adjust_size(keylen, datalen);
	enum TDB_ERROR ecode;

	add_stat(tdb, allocs, 1);
again:
	b_off = bucket_off(ftable_off, bucket);

	/* FIXME: Try non-blocking wait first, to measure contention. */
	/* Lock this bucket. */
	ecode = tdb_lock_free_bucket(tdb, b_off, TDB_LOCK_WAIT);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	best.ftable_and_len = -1ULL;
	best_off = 0;

	/* Get slack if we're after extra. */
	if (want_extra)
		multiplier = 1.5;
	else
		multiplier = 1.0;

	/* Walk the list to see if any are large enough, getting less fussy
	 * as we go. */
	off = tdb_read_off(tdb, b_off);
	if (TDB_OFF_IS_ERR(off)) {
		ecode = off;
		goto unlock_err;
	}

	while (off) {
		const struct tdb_free_record *r;
		tdb_len_t len;
		tdb_off_t next;
		int coal;

		r = tdb_access_read(tdb, off, sizeof(*r), true);
		if (TDB_PTR_IS_ERR(r)) {
			ecode = TDB_PTR_ERR(r);
			goto unlock_err;
		}

		if (frec_magic(r) != TDB_FREE_MAGIC) {
			ecode = tdb_logerr(tdb, TDB_ERR_CORRUPT, TDB_LOG_ERROR,
					   "lock_and_alloc:"
					   " %llu non-free 0x%llx",
					   (long long)off,
					   (long long)r->magic_and_prev);
			tdb_access_release(tdb, r);
			goto unlock_err;
		}

		if (frec_len(r) >= size && frec_len(r) < frec_len(&best)) {
			best_off = off;
			best = *r;
		}

		if (frec_len(&best) <= size * multiplier && best_off) {
			tdb_access_release(tdb, r);
			break;
		}

		multiplier *= 1.01;

		next = r->next;
		len = frec_len(r);
		tdb_access_release(tdb, r);

		/* Since we're going slow anyway, try coalescing here. */
		coal = coalesce(tdb, off, b_off, len);
		if (coal == 1) {
			/* This has unlocked list, restart. */
			goto again;
		}
		if (coal < 0) {
			/* This has already unlocked on error. */
			return coal;
		}
		off = next;
	}

	/* If we found anything at all, use it. */
	if (best_off) {
		struct tdb_used_record rec;
		size_t leftover;

		/* We're happy with this size: take it. */
		ecode = remove_from_list(tdb, b_off, best_off, &best);
		check_list(tdb, b_off);
		if (ecode != TDB_SUCCESS) {
			goto unlock_err;
		}

		leftover = record_leftover(keylen, datalen, want_extra,
					   frec_len(&best));

		assert(keylen + datalen + leftover <= frec_len(&best));
		/* We need to mark non-free before we drop lock, otherwise
		 * coalesce() could try to merge it! */
		ecode = set_header(tdb, &rec, magic, keylen, datalen,
				   frec_len(&best) - leftover, hashlow);
		if (ecode != TDB_SUCCESS) {
			goto unlock_err;
		}

		ecode = tdb_write_convert(tdb, best_off, &rec, sizeof(rec));
		if (ecode != TDB_SUCCESS) {
			goto unlock_err;
		}

		/* For futureproofing, we put a 0 in any unused space. */
		if (rec_extra_padding(&rec)) {
			ecode = tdb->methods->twrite(tdb, best_off + sizeof(rec)
						     + keylen + datalen, "", 1);
			if (ecode != TDB_SUCCESS) {
				goto unlock_err;
			}
		}

		/* Bucket of leftover will be <= current bucket, so nested
		 * locking is allowed. */
		if (leftover) {
			add_stat(tdb, alloc_leftover, 1);
			ecode = add_free_record(tdb,
						best_off + sizeof(rec)
						+ frec_len(&best) - leftover,
						leftover);
			if (ecode != TDB_SUCCESS) {
				best_off = ecode;
			}
		}
		tdb_unlock_free_bucket(tdb, b_off);

		return best_off;
	}

	tdb_unlock_free_bucket(tdb, b_off);
	return 0;

unlock_err:
	tdb_unlock_free_bucket(tdb, b_off);
	return ecode;
}

/* Get a free block from current free list, or 0 if none, -ve on error. */
static tdb_off_t get_free(struct tdb_context *tdb,
			  size_t keylen, size_t datalen, bool want_extra,
			  unsigned magic, unsigned hashlow)
{
	tdb_off_t off, ftable_off;
	tdb_off_t start_b, b, ftable;
	bool wrapped = false;

	/* If they are growing, add 50% to get to higher bucket. */
	if (want_extra)
		start_b = size_to_bucket(adjust_size(keylen,
						     datalen + datalen / 2));
	else
		start_b = size_to_bucket(adjust_size(keylen, datalen));

	ftable_off = tdb->ftable_off;
	ftable = tdb->ftable;
	while (!wrapped || ftable_off != tdb->ftable_off) {
		/* Start at exact size bucket, and search up... */
		for (b = find_free_head(tdb, ftable_off, start_b);
		     b < TDB_FREE_BUCKETS;
		     b = find_free_head(tdb, ftable_off, b + 1)) {
			/* Try getting one from list. */
			off = lock_and_alloc(tdb, ftable_off,
					     b, keylen, datalen, want_extra,
					     magic, hashlow);
			if (TDB_OFF_IS_ERR(off))
				return off;
			if (off != 0) {
				if (b == start_b)
					add_stat(tdb, alloc_bucket_exact, 1);
				if (b == TDB_FREE_BUCKETS - 1)
					add_stat(tdb, alloc_bucket_max, 1);
				/* Worked?  Stay using this list. */
				tdb->ftable_off = ftable_off;
				tdb->ftable = ftable;
				return off;
			}
			/* Didn't work.  Try next bucket. */
		}

		if (TDB_OFF_IS_ERR(b)) {
			return b;
		}

		/* Hmm, try next table. */
		ftable_off = next_ftable(tdb, ftable_off);
		if (TDB_OFF_IS_ERR(ftable_off)) {
			return ftable_off;
		}
		ftable++;

		if (ftable_off == 0) {
			wrapped = true;
			ftable_off = first_ftable(tdb);
			if (TDB_OFF_IS_ERR(ftable_off)) {
				return ftable_off;
			}
			ftable = 0;
		}
	}

	return 0;
}

enum TDB_ERROR set_header(struct tdb_context *tdb,
			  struct tdb_used_record *rec,
			  unsigned magic, uint64_t keylen, uint64_t datalen,
			  uint64_t actuallen, unsigned hashlow)
{
	uint64_t keybits = (fls64(keylen) + 1) / 2;

	/* Use bottom bits of hash, so it's independent of hash table size. */
	rec->magic_and_meta = (hashlow & ((1 << 11)-1))
		| ((actuallen - (keylen + datalen)) << 11)
		| (keybits << 43)
		| ((uint64_t)magic << 48);
	rec->key_and_data_len = (keylen | (datalen << (keybits*2)));

	/* Encoding can fail on big values. */
	if (rec_key_length(rec) != keylen
	    || rec_data_length(rec) != datalen
	    || rec_extra_padding(rec) != actuallen - (keylen + datalen)) {
		return tdb_logerr(tdb, TDB_ERR_IO, TDB_LOG_ERROR,
				  "Could not encode k=%llu,d=%llu,a=%llu",
				  (long long)keylen, (long long)datalen,
				  (long long)actuallen);
	}
	return TDB_SUCCESS;
}

/* Expand the database. */
static enum TDB_ERROR tdb_expand(struct tdb_context *tdb, tdb_len_t size)
{
	uint64_t old_size;
	tdb_len_t wanted;
	enum TDB_ERROR ecode;

	/* We need room for the record header too. */
	wanted = sizeof(struct tdb_used_record) + size;

	/* Need to hold a hash lock to expand DB: transactions rely on it. */
	if (!(tdb->flags & TDB_NOLOCK)
	    && !tdb->file->allrecord_lock.count && !tdb_has_hash_locks(tdb)) {
		return tdb_logerr(tdb, TDB_ERR_LOCK, TDB_LOG_ERROR,
				  "tdb_expand: must hold lock during expand");
	}

	/* always make room for at least 100 more records, and at
           least 25% more space. */
	if (size * TDB_EXTENSION_FACTOR > tdb->file->map_size / 4)
		wanted = size * TDB_EXTENSION_FACTOR;
	else
		wanted = tdb->file->map_size / 4;
	wanted = adjust_size(0, wanted);

	/* Only one person can expand file at a time. */
	ecode = tdb_lock_expand(tdb, F_WRLCK);
	if (ecode != TDB_SUCCESS) {
		return ecode;
	}

	/* Someone else may have expanded the file, so retry. */
	old_size = tdb->file->map_size;
	tdb->methods->oob(tdb, tdb->file->map_size + 1, true);
	if (tdb->file->map_size != old_size) {
		tdb_unlock_expand(tdb, F_WRLCK);
		return TDB_SUCCESS;
	}

	ecode = tdb->methods->expand_file(tdb, wanted);
	if (ecode != TDB_SUCCESS) {
		tdb_unlock_expand(tdb, F_WRLCK);
		return ecode;
	}

	/* We need to drop this lock before adding free record. */
	tdb_unlock_expand(tdb, F_WRLCK);

	add_stat(tdb, expands, 1);
	return add_free_record(tdb, old_size, wanted);
}

/* This won't fail: it will expand the database if it has to. */
tdb_off_t alloc(struct tdb_context *tdb, size_t keylen, size_t datalen,
		uint64_t hash, unsigned magic, bool growing)
{
	tdb_off_t off;

	/* We can't hold pointers during this: we could unmap! */
	assert(!tdb->direct_access);

	for (;;) {
		enum TDB_ERROR ecode;
		off = get_free(tdb, keylen, datalen, growing, magic, hash);
		if (likely(off != 0))
			break;

		ecode = tdb_expand(tdb, adjust_size(keylen, datalen));
		if (ecode != TDB_SUCCESS) {
			return ecode;
		}
	}

	return off;
}
