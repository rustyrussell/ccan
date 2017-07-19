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
#include <limits.h>

static unsigned fls64(uint64_t val)
{
	return ilog64(val);
}

/* In which bucket would we find a particular record size? (ignoring header) */
unsigned int size_to_bucket(ntdb_len_t data_len)
{
	unsigned int bucket;

	/* We can't have records smaller than this. */
	assert(data_len >= NTDB_MIN_DATA_LEN);

	/* Ignoring the header... */
	if (data_len - NTDB_MIN_DATA_LEN <= 64) {
		/* 0 in bucket 0, 8 in bucket 1... 64 in bucket 8. */
		bucket = (data_len - NTDB_MIN_DATA_LEN) / 8;
	} else {
		/* After that we go power of 2. */
		bucket = fls64(data_len - NTDB_MIN_DATA_LEN) + 2;
	}

	if (unlikely(bucket >= NTDB_FREE_BUCKETS))
		bucket = NTDB_FREE_BUCKETS - 1;
	return bucket;
}

ntdb_off_t first_ftable(struct ntdb_context *ntdb)
{
	return ntdb_read_off(ntdb, offsetof(struct ntdb_header, free_table));
}

ntdb_off_t next_ftable(struct ntdb_context *ntdb, ntdb_off_t ftable)
{
	return ntdb_read_off(ntdb, ftable + offsetof(struct ntdb_freetable,next));
}

enum NTDB_ERROR ntdb_ftable_init(struct ntdb_context *ntdb)
{
	/* Use reservoir sampling algorithm to select a free list at random. */
	unsigned int rnd, max = 0, count = 0;
	ntdb_off_t off;

	ntdb->ftable_off = off = first_ftable(ntdb);
	ntdb->ftable = 0;

	while (off) {
		if (NTDB_OFF_IS_ERR(off)) {
			return NTDB_OFF_TO_ERR(off);
		}

		rnd = random();
		if (rnd >= max) {
			ntdb->ftable_off = off;
			ntdb->ftable = count;
			max = rnd;
		}

		off = next_ftable(ntdb, off);
		count++;
	}
	return NTDB_SUCCESS;
}

/* Offset of a given bucket. */
ntdb_off_t bucket_off(ntdb_off_t ftable_off, unsigned bucket)
{
	return ftable_off + offsetof(struct ntdb_freetable, buckets)
		+ bucket * sizeof(ntdb_off_t);
}

/* Returns free_buckets + 1, or list number to search, or -ve error. */
static ntdb_off_t find_free_head(struct ntdb_context *ntdb,
				ntdb_off_t ftable_off,
				ntdb_off_t bucket)
{
	/* Speculatively search for a non-zero bucket. */
	return ntdb_find_nonzero_off(ntdb, bucket_off(ftable_off, 0),
				    bucket, NTDB_FREE_BUCKETS);
}

static void check_list(struct ntdb_context *ntdb, ntdb_off_t b_off)
{
#ifdef CCAN_NTDB_DEBUG
	ntdb_off_t off, prev = 0, first;
	struct ntdb_free_record r;

	first = off = (ntdb_read_off(ntdb, b_off) & NTDB_OFF_MASK);
	while (off != 0) {
		ntdb_read_convert(ntdb, off, &r, sizeof(r));
		if (frec_magic(&r) != NTDB_FREE_MAGIC)
			abort();
		if (prev && frec_prev(&r) != prev)
			abort();
		prev = off;
		off = r.next;
	}

	if (first) {
		ntdb_read_convert(ntdb, first, &r, sizeof(r));
		if (frec_prev(&r) != prev)
			abort();
	}
#endif
}

/* Remove from free bucket. */
static enum NTDB_ERROR remove_from_list(struct ntdb_context *ntdb,
				       ntdb_off_t b_off, ntdb_off_t r_off,
				       const struct ntdb_free_record *r)
{
	ntdb_off_t off, prev_next, head;
	enum NTDB_ERROR ecode;

	/* Is this only element in list?  Zero out bucket, and we're done. */
	if (frec_prev(r) == r_off)
		return ntdb_write_off(ntdb, b_off, 0);

	/* off = &r->prev->next */
	off = frec_prev(r) + offsetof(struct ntdb_free_record, next);

	/* Get prev->next */
	prev_next = ntdb_read_off(ntdb, off);
	if (NTDB_OFF_IS_ERR(prev_next))
		return NTDB_OFF_TO_ERR(prev_next);

	/* If prev->next == 0, we were head: update bucket to point to next. */
	if (prev_next == 0) {
		/* We must preserve upper bits. */
		head = ntdb_read_off(ntdb, b_off);
		if (NTDB_OFF_IS_ERR(head))
			return NTDB_OFF_TO_ERR(head);

		if ((head & NTDB_OFF_MASK) != r_off) {
			return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
					  "remove_from_list:"
					  " %llu head %llu on list %llu",
					  (long long)r_off,
					  (long long)head,
					  (long long)b_off);
		}
		head = ((head & ~NTDB_OFF_MASK) | r->next);
		ecode = ntdb_write_off(ntdb, b_off, head);
		if (ecode != NTDB_SUCCESS)
			return ecode;
	} else {
		/* r->prev->next = r->next */
		ecode = ntdb_write_off(ntdb, off, r->next);
		if (ecode != NTDB_SUCCESS)
			return ecode;
	}

	/* If we were the tail, off = &head->prev. */
	if (r->next == 0) {
		head = ntdb_read_off(ntdb, b_off);
		if (NTDB_OFF_IS_ERR(head))
			return NTDB_OFF_TO_ERR(head);
		head &= NTDB_OFF_MASK;
		off = head + offsetof(struct ntdb_free_record, magic_and_prev);
	} else {
		/* off = &r->next->prev */
		off = r->next + offsetof(struct ntdb_free_record,
					 magic_and_prev);
	}

#ifdef CCAN_NTDB_DEBUG
	/* *off == r */
	if ((ntdb_read_off(ntdb, off) & NTDB_OFF_MASK) != r_off) {
		return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
				  "remove_from_list:"
				  " %llu bad prev in list %llu",
				  (long long)r_off, (long long)b_off);
	}
#endif
	/* r->next->prev = r->prev */
	return ntdb_write_off(ntdb, off, r->magic_and_prev);
}

/* Enqueue in this free bucket: sets coalesce if we've added 128
 * entries to it. */
static enum NTDB_ERROR enqueue_in_free(struct ntdb_context *ntdb,
				      ntdb_off_t b_off,
				      ntdb_off_t off,
				      ntdb_len_t len,
				      bool *coalesce)
{
	struct ntdb_free_record new;
	enum NTDB_ERROR ecode;
	ntdb_off_t prev, head;
	uint64_t magic = (NTDB_FREE_MAGIC << (64 - NTDB_OFF_UPPER_STEAL));

	head = ntdb_read_off(ntdb, b_off);
	if (NTDB_OFF_IS_ERR(head))
		return NTDB_OFF_TO_ERR(head);

	/* We only need to set ftable_and_len; rest is set in enqueue_in_free */
	new.ftable_and_len = ((uint64_t)ntdb->ftable
			      << (64 - NTDB_OFF_UPPER_STEAL))
		| len;

	/* new->next = head. */
	new.next = (head & NTDB_OFF_MASK);

	/* First element?  Prev points to ourselves. */
	if (!new.next) {
		new.magic_and_prev = (magic | off);
	} else {
		/* new->prev = next->prev */
		prev = ntdb_read_off(ntdb,
				    new.next + offsetof(struct ntdb_free_record,
							magic_and_prev));
		new.magic_and_prev = prev;
		if (frec_magic(&new) != NTDB_FREE_MAGIC) {
			return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
					  "enqueue_in_free: %llu bad head"
					  " prev %llu",
					  (long long)new.next,
					  (long long)prev);
		}
		/* next->prev = new. */
		ecode = ntdb_write_off(ntdb, new.next
				      + offsetof(struct ntdb_free_record,
						 magic_and_prev),
				      off | magic);
		if (ecode != NTDB_SUCCESS) {
			return ecode;
		}

#ifdef CCAN_NTDB_DEBUG
		prev = ntdb_read_off(ntdb, frec_prev(&new)
				    + offsetof(struct ntdb_free_record, next));
		if (prev != 0) {
			return ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
					  "enqueue_in_free:"
					  " %llu bad tail next ptr %llu",
					  (long long)frec_prev(&new)
					  + offsetof(struct ntdb_free_record,
						     next),
					  (long long)prev);
		}
#endif
	}

	/* Update enqueue count, but don't set high bit: see NTDB_OFF_IS_ERR */
	if (*coalesce)
		head += (1ULL << (64 - NTDB_OFF_UPPER_STEAL));
	head &= ~(NTDB_OFF_MASK | (1ULL << 63));
	head |= off;

	ecode = ntdb_write_off(ntdb, b_off, head);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* It's time to coalesce if counter wrapped. */
	if (*coalesce)
		*coalesce = ((head & ~NTDB_OFF_MASK) == 0);

	return ntdb_write_convert(ntdb, off, &new, sizeof(new));
}

static ntdb_off_t ftable_offset(struct ntdb_context *ntdb, unsigned int ftable)
{
	ntdb_off_t off;
	unsigned int i;

	if (likely(ntdb->ftable == ftable))
		return ntdb->ftable_off;

	off = first_ftable(ntdb);
	for (i = 0; i < ftable; i++) {
		if (NTDB_OFF_IS_ERR(off)) {
			break;
		}
		off = next_ftable(ntdb, off);
	}
	return off;
}

/* Note: we unlock the current bucket if fail (-ve), or coalesce (+ve) and
 * need to blatt the *protect record (which is set to an error). */
static ntdb_len_t coalesce(struct ntdb_context *ntdb,
			  ntdb_off_t off, ntdb_off_t b_off,
			  ntdb_len_t data_len,
			  ntdb_off_t *protect)
{
	ntdb_off_t end;
	struct ntdb_free_record rec;
	enum NTDB_ERROR ecode;

	ntdb->stats.alloc_coalesce_tried++;
	end = off + sizeof(struct ntdb_used_record) + data_len;

	while (end < ntdb->file->map_size) {
		const struct ntdb_free_record *r;
		ntdb_off_t nb_off;
		unsigned ftable, bucket;

		r = ntdb_access_read(ntdb, end, sizeof(*r), true);
		if (NTDB_PTR_IS_ERR(r)) {
			ecode = NTDB_PTR_ERR(r);
			goto err;
		}

		if (frec_magic(r) != NTDB_FREE_MAGIC
		    || frec_ftable(r) == NTDB_FTABLE_NONE) {
			ntdb_access_release(ntdb, r);
			break;
		}

		ftable = frec_ftable(r);
		bucket = size_to_bucket(frec_len(r));
		nb_off = ftable_offset(ntdb, ftable);
		if (NTDB_OFF_IS_ERR(nb_off)) {
			ntdb_access_release(ntdb, r);
			ecode = NTDB_OFF_TO_ERR(nb_off);
			goto err;
		}
		nb_off = bucket_off(nb_off, bucket);
		ntdb_access_release(ntdb, r);

		/* We may be violating lock order here, so best effort. */
		if (ntdb_lock_free_bucket(ntdb, nb_off, NTDB_LOCK_NOWAIT)
		    != NTDB_SUCCESS) {
			ntdb->stats.alloc_coalesce_lockfail++;
			break;
		}

		/* Now we have lock, re-check. */
		ecode = ntdb_read_convert(ntdb, end, &rec, sizeof(rec));
		if (ecode != NTDB_SUCCESS) {
			ntdb_unlock_free_bucket(ntdb, nb_off);
			goto err;
		}

		if (unlikely(frec_magic(&rec) != NTDB_FREE_MAGIC)) {
			ntdb->stats.alloc_coalesce_race++;
			ntdb_unlock_free_bucket(ntdb, nb_off);
			break;
		}

		if (unlikely(frec_ftable(&rec) != ftable)
		    || unlikely(size_to_bucket(frec_len(&rec)) != bucket)) {
			ntdb->stats.alloc_coalesce_race++;
			ntdb_unlock_free_bucket(ntdb, nb_off);
			break;
		}

		/* Did we just mess up a record you were hoping to use? */
		if (end == *protect) {
			ntdb->stats.alloc_coalesce_iterate_clash++;
			*protect = NTDB_ERR_TO_OFF(NTDB_ERR_NOEXIST);
		}

		ecode = remove_from_list(ntdb, nb_off, end, &rec);
		check_list(ntdb, nb_off);
		if (ecode != NTDB_SUCCESS) {
			ntdb_unlock_free_bucket(ntdb, nb_off);
			goto err;
		}

		end += sizeof(struct ntdb_used_record) + frec_len(&rec);
		ntdb_unlock_free_bucket(ntdb, nb_off);
		ntdb->stats.alloc_coalesce_num_merged++;
	}

	/* Didn't find any adjacent free? */
	if (end == off + sizeof(struct ntdb_used_record) + data_len)
		return 0;

	/* Before we expand, check this isn't one you wanted protected? */
	if (off == *protect) {
		*protect = NTDB_ERR_TO_OFF(NTDB_ERR_EXISTS);
		ntdb->stats.alloc_coalesce_iterate_clash++;
	}

	/* OK, expand initial record */
	ecode = ntdb_read_convert(ntdb, off, &rec, sizeof(rec));
	if (ecode != NTDB_SUCCESS) {
		goto err;
	}

	if (frec_len(&rec) != data_len) {
		ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
				   "coalesce: expected data len %zu not %zu",
				   (size_t)data_len, (size_t)frec_len(&rec));
		goto err;
	}

	ecode = remove_from_list(ntdb, b_off, off, &rec);
	check_list(ntdb, b_off);
	if (ecode != NTDB_SUCCESS) {
		goto err;
	}

	/* Try locking violation first.  We don't allow coalesce recursion! */
	ecode = add_free_record(ntdb, off, end - off, NTDB_LOCK_NOWAIT, false);
	if (ecode != NTDB_SUCCESS) {
		/* Need to drop lock.  Can't rely on anything stable. */
		ntdb->stats.alloc_coalesce_lockfail++;
		*protect = NTDB_ERR_TO_OFF(NTDB_ERR_CORRUPT);

		/* We have to drop this to avoid deadlocks, so make sure record
		 * doesn't get coalesced by someone else! */
		rec.ftable_and_len = (NTDB_FTABLE_NONE
				      << (64 - NTDB_OFF_UPPER_STEAL))
			| (end - off - sizeof(struct ntdb_used_record));
		ecode = ntdb_write_off(ntdb,
				      off + offsetof(struct ntdb_free_record,
						     ftable_and_len),
				      rec.ftable_and_len);
		if (ecode != NTDB_SUCCESS) {
			goto err;
		}

		ntdb_unlock_free_bucket(ntdb, b_off);

		ecode = add_free_record(ntdb, off, end - off, NTDB_LOCK_WAIT,
					false);
		if (ecode != NTDB_SUCCESS) {
			return NTDB_ERR_TO_OFF(ecode);
		}
	} else if (NTDB_OFF_IS_ERR(*protect)) {
		/* For simplicity, we always drop lock if they can't continue */
		ntdb_unlock_free_bucket(ntdb, b_off);
	}
	ntdb->stats.alloc_coalesce_succeeded++;

	/* Return usable length. */
	return end - off - sizeof(struct ntdb_used_record);

err:
	/* To unify error paths, we *always* unlock bucket on error. */
	ntdb_unlock_free_bucket(ntdb, b_off);
	return NTDB_ERR_TO_OFF(ecode);
}

/* List is locked: we unlock it. */
static enum NTDB_ERROR coalesce_list(struct ntdb_context *ntdb,
				    ntdb_off_t ftable_off,
				    ntdb_off_t b_off,
				    unsigned int limit)
{
	enum NTDB_ERROR ecode;
	ntdb_off_t off;

	off = ntdb_read_off(ntdb, b_off);
	if (NTDB_OFF_IS_ERR(off)) {
		ecode = NTDB_OFF_TO_ERR(off);
		goto unlock_err;
	}
	/* A little bit of paranoia: counter should be 0. */
	off &= NTDB_OFF_MASK;

	while (off && limit--) {
		struct ntdb_free_record rec;
		ntdb_len_t coal;
		ntdb_off_t next;

		ecode = ntdb_read_convert(ntdb, off, &rec, sizeof(rec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		next = rec.next;
		coal = coalesce(ntdb, off, b_off, frec_len(&rec), &next);
		if (NTDB_OFF_IS_ERR(coal)) {
			/* This has already unlocked on error. */
			return NTDB_OFF_TO_ERR(coal);
		}
		if (NTDB_OFF_IS_ERR(next)) {
			/* Coalescing had to unlock, so stop. */
			return NTDB_SUCCESS;
		}
		/* Keep going if we're doing well... */
		limit += size_to_bucket(coal / 16 + NTDB_MIN_DATA_LEN);
		off = next;
	}

	/* Now, move those elements to the tail of the list so we get something
	 * else next time. */
	if (off) {
		struct ntdb_free_record oldhrec, newhrec, oldtrec, newtrec;
		ntdb_off_t oldhoff, oldtoff, newtoff;

		/* The record we were up to is the new head. */
		ecode = ntdb_read_convert(ntdb, off, &newhrec, sizeof(newhrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		/* Get the new tail. */
		newtoff = frec_prev(&newhrec);
		ecode = ntdb_read_convert(ntdb, newtoff, &newtrec,
					 sizeof(newtrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		/* Get the old head. */
		oldhoff = ntdb_read_off(ntdb, b_off);
		if (NTDB_OFF_IS_ERR(oldhoff)) {
			ecode = NTDB_OFF_TO_ERR(oldhoff);
			goto unlock_err;
		}

		/* This could happen if they all coalesced away. */
		if (oldhoff == off)
			goto out;

		ecode = ntdb_read_convert(ntdb, oldhoff, &oldhrec,
					 sizeof(oldhrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		/* Get the old tail. */
		oldtoff = frec_prev(&oldhrec);
		ecode = ntdb_read_convert(ntdb, oldtoff, &oldtrec,
					 sizeof(oldtrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		/* Old tail's next points to old head. */
		oldtrec.next = oldhoff;

		/* Old head's prev points to old tail. */
		oldhrec.magic_and_prev
			= (NTDB_FREE_MAGIC << (64 - NTDB_OFF_UPPER_STEAL))
			| oldtoff;

		/* New tail's next is 0. */
		newtrec.next = 0;

		/* Write out the modified versions. */
		ecode = ntdb_write_convert(ntdb, oldtoff, &oldtrec,
					  sizeof(oldtrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		ecode = ntdb_write_convert(ntdb, oldhoff, &oldhrec,
					  sizeof(oldhrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		ecode = ntdb_write_convert(ntdb, newtoff, &newtrec,
					  sizeof(newtrec));
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;

		/* And finally link in new head. */
		ecode = ntdb_write_off(ntdb, b_off, off);
		if (ecode != NTDB_SUCCESS)
			goto unlock_err;
	}
out:
	ntdb_unlock_free_bucket(ntdb, b_off);
	return NTDB_SUCCESS;

unlock_err:
	ntdb_unlock_free_bucket(ntdb, b_off);
	return ecode;
}

/* List must not be locked if coalesce_ok is set. */
enum NTDB_ERROR add_free_record(struct ntdb_context *ntdb,
			       ntdb_off_t off, ntdb_len_t len_with_header,
			       enum ntdb_lock_flags waitflag,
			       bool coalesce_ok)
{
	ntdb_off_t b_off;
	ntdb_len_t len;
	enum NTDB_ERROR ecode;

	assert(len_with_header >= sizeof(struct ntdb_free_record));

	len = len_with_header - sizeof(struct ntdb_used_record);

	b_off = bucket_off(ntdb->ftable_off, size_to_bucket(len));
	ecode = ntdb_lock_free_bucket(ntdb, b_off, waitflag);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	ecode = enqueue_in_free(ntdb, b_off, off, len, &coalesce_ok);
	check_list(ntdb, b_off);

	/* Coalescing unlocks free list. */
	if (!ecode && coalesce_ok)
		ecode = coalesce_list(ntdb, ntdb->ftable_off, b_off, 2);
	else
		ntdb_unlock_free_bucket(ntdb, b_off);
	return ecode;
}

static size_t adjust_size(size_t keylen, size_t datalen)
{
	size_t size = keylen + datalen;

	if (size < NTDB_MIN_DATA_LEN)
		size = NTDB_MIN_DATA_LEN;

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

	if (leftover < (ssize_t)sizeof(struct ntdb_free_record))
		return 0;

	return leftover;
}

/* We need size bytes to put our key and data in. */
static ntdb_off_t lock_and_alloc(struct ntdb_context *ntdb,
				ntdb_off_t ftable_off,
				ntdb_off_t bucket,
				size_t keylen, size_t datalen,
				bool want_extra,
				unsigned magic)
{
	ntdb_off_t off, b_off,best_off;
	struct ntdb_free_record best = { 0 };
	double multiplier;
	size_t size = adjust_size(keylen, datalen);
	enum NTDB_ERROR ecode;

	ntdb->stats.allocs++;
	b_off = bucket_off(ftable_off, bucket);

	/* FIXME: Try non-blocking wait first, to measure contention. */
	/* Lock this bucket. */
	ecode = ntdb_lock_free_bucket(ntdb, b_off, NTDB_LOCK_WAIT);
	if (ecode != NTDB_SUCCESS) {
		return NTDB_ERR_TO_OFF(ecode);
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
	off = ntdb_read_off(ntdb, b_off);
	if (NTDB_OFF_IS_ERR(off)) {
		ecode = NTDB_OFF_TO_ERR(off);
		goto unlock_err;
	}
	off &= NTDB_OFF_MASK;

	while (off) {
		const struct ntdb_free_record *r;
		ntdb_off_t next;

		r = ntdb_access_read(ntdb, off, sizeof(*r), true);
		if (NTDB_PTR_IS_ERR(r)) {
			ecode = NTDB_PTR_ERR(r);
			goto unlock_err;
		}

		if (frec_magic(r) != NTDB_FREE_MAGIC) {
			ecode = ntdb_logerr(ntdb, NTDB_ERR_CORRUPT, NTDB_LOG_ERROR,
					   "lock_and_alloc:"
					   " %llu non-free 0x%llx",
					   (long long)off,
					   (long long)r->magic_and_prev);
			ntdb_access_release(ntdb, r);
			goto unlock_err;
		}

		if (frec_len(r) >= size && frec_len(r) < frec_len(&best)) {
			best_off = off;
			best = *r;
		}

		if (frec_len(&best) <= size * multiplier && best_off) {
			ntdb_access_release(ntdb, r);
			break;
		}

		multiplier *= 1.01;

		next = r->next;
		ntdb_access_release(ntdb, r);
		off = next;
	}

	/* If we found anything at all, use it. */
	if (best_off) {
		struct ntdb_used_record rec;
		size_t leftover;

		/* We're happy with this size: take it. */
		ecode = remove_from_list(ntdb, b_off, best_off, &best);
		check_list(ntdb, b_off);
		if (ecode != NTDB_SUCCESS) {
			goto unlock_err;
		}

		leftover = record_leftover(keylen, datalen, want_extra,
					   frec_len(&best));

		assert(keylen + datalen + leftover <= frec_len(&best));
		/* We need to mark non-free before we drop lock, otherwise
		 * coalesce() could try to merge it! */
		ecode = set_header(ntdb, &rec, magic, keylen, datalen,
				   frec_len(&best) - leftover);
		if (ecode != NTDB_SUCCESS) {
			goto unlock_err;
		}

		ecode = ntdb_write_convert(ntdb, best_off, &rec, sizeof(rec));
		if (ecode != NTDB_SUCCESS) {
			goto unlock_err;
		}

		/* For futureproofing, we put a 0 in any unused space. */
		if (rec_extra_padding(&rec)) {
			ecode = ntdb->io->twrite(ntdb, best_off + sizeof(rec)
						+ keylen + datalen, "", 1);
			if (ecode != NTDB_SUCCESS) {
				goto unlock_err;
			}
		}

		/* Bucket of leftover will be <= current bucket, so nested
		 * locking is allowed. */
		if (leftover) {
			ntdb->stats.alloc_leftover++;
			ecode = add_free_record(ntdb,
						best_off + sizeof(rec)
						+ frec_len(&best) - leftover,
						leftover, NTDB_LOCK_WAIT, false);
			if (ecode != NTDB_SUCCESS) {
				best_off = NTDB_ERR_TO_OFF(ecode);
			}
		}
		ntdb_unlock_free_bucket(ntdb, b_off);

		return best_off;
	}

	ntdb_unlock_free_bucket(ntdb, b_off);
	return 0;

unlock_err:
	ntdb_unlock_free_bucket(ntdb, b_off);
	return NTDB_ERR_TO_OFF(ecode);
}

/* Get a free block from current free list, or 0 if none, -ve on error. */
static ntdb_off_t get_free(struct ntdb_context *ntdb,
			  size_t keylen, size_t datalen, bool want_extra,
			  unsigned magic)
{
	ntdb_off_t off, ftable_off;
	ntdb_off_t start_b, b, ftable;
	bool wrapped = false;

	/* If they are growing, add 50% to get to higher bucket. */
	if (want_extra)
		start_b = size_to_bucket(adjust_size(keylen,
						     datalen + datalen / 2));
	else
		start_b = size_to_bucket(adjust_size(keylen, datalen));

	ftable_off = ntdb->ftable_off;
	ftable = ntdb->ftable;
	while (!wrapped || ftable_off != ntdb->ftable_off) {
		/* Start at exact size bucket, and search up... */
		for (b = find_free_head(ntdb, ftable_off, start_b);
		     b < NTDB_FREE_BUCKETS;
		     b = find_free_head(ntdb, ftable_off, b + 1)) {
			/* Try getting one from list. */
			off = lock_and_alloc(ntdb, ftable_off,
					     b, keylen, datalen, want_extra,
					     magic);
			if (NTDB_OFF_IS_ERR(off))
				return off;
			if (off != 0) {
				if (b == start_b)
					ntdb->stats.alloc_bucket_exact++;
				if (b == NTDB_FREE_BUCKETS - 1)
					ntdb->stats.alloc_bucket_max++;
				/* Worked?  Stay using this list. */
				ntdb->ftable_off = ftable_off;
				ntdb->ftable = ftable;
				return off;
			}
			/* Didn't work.  Try next bucket. */
		}

		if (NTDB_OFF_IS_ERR(b)) {
			return b;
		}

		/* Hmm, try next table. */
		ftable_off = next_ftable(ntdb, ftable_off);
		if (NTDB_OFF_IS_ERR(ftable_off)) {
			return ftable_off;
		}
		ftable++;

		if (ftable_off == 0) {
			wrapped = true;
			ftable_off = first_ftable(ntdb);
			if (NTDB_OFF_IS_ERR(ftable_off)) {
				return ftable_off;
			}
			ftable = 0;
		}
	}

	return 0;
}

enum NTDB_ERROR set_header(struct ntdb_context *ntdb,
			  struct ntdb_used_record *rec,
			  unsigned magic, uint64_t keylen, uint64_t datalen,
			  uint64_t actuallen)
{
	uint64_t keybits = (fls64(keylen) + 1) / 2;

	rec->magic_and_meta = ((actuallen - (keylen + datalen)) << 11)
		| (keybits << 43)
		| ((uint64_t)magic << 48);
	rec->key_and_data_len = (keylen | (datalen << (keybits*2)));

	/* Encoding can fail on big values. */
	if (rec_key_length(rec) != keylen
	    || rec_data_length(rec) != datalen
	    || rec_extra_padding(rec) != actuallen - (keylen + datalen)) {
		return ntdb_logerr(ntdb, NTDB_ERR_IO, NTDB_LOG_ERROR,
				  "Could not encode k=%llu,d=%llu,a=%llu",
				  (long long)keylen, (long long)datalen,
				  (long long)actuallen);
	}
	return NTDB_SUCCESS;
}

/* You need 'size', this tells you how much you should expand by. */
ntdb_off_t ntdb_expand_adjust(ntdb_off_t map_size, ntdb_off_t size)
{
	ntdb_off_t new_size, top_size;

	/* limit size in order to avoid using up huge amounts of memory for
	 * in memory tdbs if an oddball huge record creeps in */
	if (size > 100 * 1024) {
		top_size = map_size + size * 2;
	} else {
		top_size = map_size + size * 100;
	}

	/* always make room for at least top_size more records, and at
	   least 25% more space. if the DB is smaller than 100MiB,
	   otherwise grow it by 10% only. */
	if (map_size > 100 * 1024 * 1024) {
		new_size = map_size * 1.10;
	} else {
		new_size = map_size * 1.25;
	}

	if (new_size < top_size)
		new_size = top_size;

	/* We always make the file a multiple of transaction page
	 * size.  This guarantees that the transaction recovery area
	 * is always aligned, otherwise the transaction code can overwrite
	 * itself. */
	new_size = (new_size + NTDB_PGSIZE-1) & ~(NTDB_PGSIZE-1);
	return new_size - map_size;
}

/* Expand the database. */
static enum NTDB_ERROR ntdb_expand(struct ntdb_context *ntdb, ntdb_len_t size)
{
	uint64_t old_size;
	ntdb_len_t wanted;
	enum NTDB_ERROR ecode;

	/* Need to hold a hash lock to expand DB: transactions rely on it. */
	if (!(ntdb->flags & NTDB_NOLOCK)
	    && !ntdb->file->allrecord_lock.count && !ntdb_has_hash_locks(ntdb)) {
		return ntdb_logerr(ntdb, NTDB_ERR_LOCK, NTDB_LOG_ERROR,
				  "ntdb_expand: must hold lock during expand");
	}

	/* Only one person can expand file at a time. */
	ecode = ntdb_lock_expand(ntdb, F_WRLCK);
	if (ecode != NTDB_SUCCESS) {
		return ecode;
	}

	/* Someone else may have expanded the file, so retry. */
	old_size = ntdb->file->map_size;
	ntdb_oob(ntdb, ntdb->file->map_size, 1, true);
	if (ntdb->file->map_size != old_size) {
		ntdb_unlock_expand(ntdb, F_WRLCK);
		return NTDB_SUCCESS;
	}

	/* We need room for the record header too. */
	size = adjust_size(0, sizeof(struct ntdb_used_record) + size);
	/* Overallocate. */
	wanted = ntdb_expand_adjust(old_size, size);

	ecode = ntdb->io->expand_file(ntdb, wanted);
	if (ecode != NTDB_SUCCESS) {
		ntdb_unlock_expand(ntdb, F_WRLCK);
		return ecode;
	}

	/* We need to drop this lock before adding free record. */
	ntdb_unlock_expand(ntdb, F_WRLCK);

	ntdb->stats.expands++;
	return add_free_record(ntdb, old_size, wanted, NTDB_LOCK_WAIT, true);
}

/* This won't fail: it will expand the database if it has to. */
ntdb_off_t alloc(struct ntdb_context *ntdb, size_t keylen, size_t datalen,
		 unsigned magic, bool growing)
{
	ntdb_off_t off;

	for (;;) {
		enum NTDB_ERROR ecode;
		off = get_free(ntdb, keylen, datalen, growing, magic);
		if (likely(off != 0))
			break;

		ecode = ntdb_expand(ntdb, adjust_size(keylen, datalen));
		if (ecode != NTDB_SUCCESS) {
			return NTDB_ERR_TO_OFF(ecode);
		}
	}

	return off;
}
