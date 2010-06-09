#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include "alloc.h"
#include <ccan/build_assert/build_assert.h>
#include <ccan/likely/likely.h>
#include <ccan/short_types/short_types.h>
#include "config.h"

/*
   Inspired by (and parts taken from) Andrew Tridgell's alloc_mmap:
   http://samba.org/~tridge/junkcode/alloc_mmap/

   Copyright (C) Andrew Tridgell 2007
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#if 0 /* Until we have the tiny allocator working, go down to 1 MB */

/* We divide the pool into this many large pages (nearest power of 2) */
#define MAX_PAGES (1024UL)

/* 32 small pages == 1 large page. */
#define BITS_FROM_SMALL_TO_LARGE_PAGE 5

#else

#define MAX_PAGES (128UL)
#define BITS_FROM_SMALL_TO_LARGE_PAGE 4

#endif

/* Smallest pool size for this scheme: 512-byte small pages.  That's
 * 4/8% overhead for 32/64 bit. */
#define MIN_USEFUL_SIZE (MAX_PAGES << (9 + BITS_FROM_SMALL_TO_LARGE_PAGE))

/* Every 4 buckets, we jump up a power of 2. ...8 10 12 14 16 20 24 28 32... */
#define INTER_BUCKET_SPACE 4

/* FIXME: Figure this out properly. */
#define MAX_SIZE (1 << 30)

/* How few object to fit in a page before using a larger one? (8) */
#define MAX_PAGE_OBJECT_ORDER	3

#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)

struct bucket_state {
	unsigned long elements_per_page;
	unsigned long page_list;
	unsigned long full_list;
};

struct header {
	/* 1024 bit bitmap of which pages are large. */
	unsigned long pagesize[MAX_PAGES / BITS_PER_LONG];

	/* List of unused small/large pages. */
	unsigned long small_free_list;
	unsigned long large_free_list;

	/* This is less defined: we have two buckets for each power of 2 */
	struct bucket_state bs[1];
};

struct page_header {
	unsigned long next, prev;
	u32 elements_used;
	/* FIXME: Pack this in somewhere... */
	u8 bucket;
	unsigned long used[1]; /* One bit per element. */
};

/* 2 bit for every byte to allocate. */
static void tiny_alloc_init(void *pool, unsigned long poolsize)
{
/* FIXME */
}

static void *tiny_alloc_get(void *pool, unsigned long poolsize,
			    unsigned long size, unsigned long align)
{
/* FIXME */
	return NULL;
}

static void tiny_alloc_free(void *pool, unsigned long poolsize, void *free)
{
/* FIXME */
}

static unsigned long tiny_alloc_size(void *pool, unsigned long poolsize,
				      void *p)
{
/* FIXME */
	return 0;
}

static bool tiny_alloc_check(void *pool, unsigned long poolsize)
{
/* FIXME */
	return true;
}

static unsigned int fls(unsigned long val)
{
#if HAVE_BUILTIN_CLZL
	/* This is significantly faster! */
	return val ? sizeof(long) * CHAR_BIT - __builtin_clzl(val) : 0;
#else
	unsigned int r = 32;

	if (!val)
		return 0;
	if (!(val & 0xffff0000u)) {
		val <<= 16;
		r -= 16;
	}
	if (!(val & 0xff000000u)) {
		val <<= 8;
		r -= 8;
	}
	if (!(val & 0xf0000000u)) {
		val <<= 4;
		r -= 4;
	}
	if (!(val & 0xc0000000u)) {
		val <<= 2;
		r -= 2;
	}
	if (!(val & 0x80000000u)) {
		val <<= 1;
		r -= 1;
	}
	return r;
#endif
}

/* FIXME: Move to bitops. */
static unsigned int ffsl(unsigned long val)
{
#if HAVE_BUILTIN_FFSL
	/* This is significantly faster! */
	return __builtin_ffsl(val);
#else
	unsigned int r = 1;

	if (!val)
		return 0;
	if (sizeof(long) == sizeof(u64)) {
		if (!(val & 0xffffffff)) {
			/* Workaround gcc warning on 32-bit:
			   error: right shift count >= width of type */
			u64 tmp = val;
			tmp >>= 32;
			val = tmp;
			r += 32;
		}
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
	if (!(val & 3)) {
		val >>= 2;
		r += 2;
	}
	if (!(val & 1)) {
		val >>= 1;
		r += 1;
	}
	return r;
#endif
}

static unsigned int popcount(unsigned long val)
{
#if HAVE_BUILTIN_POPCOUNTL
	return __builtin_popcountl(val);
#else
	if (sizeof(long) == sizeof(u64)) {
		u64 v = val;
		v = (v & 0x5555555555555555ULL)
			+ ((v >> 1) & 0x5555555555555555ULL);
		v = (v & 0x3333333333333333ULL)
			+ ((v >> 1) & 0x3333333333333333ULL);
		v = (v & 0x0F0F0F0F0F0F0F0FULL)
			+ ((v >> 1) & 0x0F0F0F0F0F0F0F0FULL);
		v = (v & 0x00FF00FF00FF00FFULL)
			+ ((v >> 1) & 0x00FF00FF00FF00FFULL);
		v = (v & 0x0000FFFF0000FFFFULL)
			+ ((v >> 1) & 0x0000FFFF0000FFFFULL);
		v = (v & 0x00000000FFFFFFFFULL)
			+ ((v >> 1) & 0x00000000FFFFFFFFULL);
		return v;
	}
	val = (val & 0x55555555ULL) + ((val >> 1) & 0x55555555ULL);
	val = (val & 0x33333333ULL) + ((val >> 1) & 0x33333333ULL);
	val = (val & 0x0F0F0F0FULL) + ((val >> 1) & 0x0F0F0F0FULL);
	val = (val & 0x00FF00FFULL) + ((val >> 1) & 0x00FF00FFULL);
	val = (val & 0x0000FFFFULL) + ((val >> 1) & 0x0000FFFFULL);
	return val;
#endif
}

/*
 * Every 4 buckets, the size doubles.
 * Between buckets, sizes increase linearly.
 *
 * eg. bucket 40 = 2^10			= 1024
 *     bucket 41 = 2^10 + 2^10*4	= 1024 + 256
 *     bucket 42 = 2^10 + 2^10*4	= 1024 + 512
 *     bucket 43 = 2^10 + 2^10*4	= 1024 + 768
 *     bucket 45 = 2^11			= 2048
 *
 * Care is taken to handle low numbered buckets, at cost of overflow.
 */
static unsigned long bucket_to_size(unsigned int bucket)
{
	unsigned long base = 1 << (bucket / INTER_BUCKET_SPACE);
	return base + ((bucket % INTER_BUCKET_SPACE)
		       << (bucket / INTER_BUCKET_SPACE))
		/ INTER_BUCKET_SPACE;
}

/*
 * Say size is 10.
 *   fls(size/2) == 3.  1 << 3 == 8, so we're 2 too large, out of a possible
 * 8 too large.  That's 1/4 of the way to the next power of 2 == 1 bucket.
 *
 * We make sure we round up.  Note that this fails on 32 bit at size
 * 1879048193 (around bucket 120).
 */
static unsigned int size_to_bucket(unsigned long size)
{
	unsigned int base = fls(size/2);
	unsigned long overshoot;

	overshoot = size - (1 << base);
	return base * INTER_BUCKET_SPACE
		+ ((overshoot * INTER_BUCKET_SPACE + (1 << base)-1) >> base);
}

static unsigned int large_page_bits(unsigned long poolsize)
{
	return fls(poolsize / MAX_PAGES / 2);
}

static unsigned long align_up(unsigned long x, unsigned long align)
{
	return (x + align - 1) & ~(align - 1);
}

static struct page_header *from_off(struct header *head, unsigned long off)
{
	return (struct page_header *)((char *)head + off);
}

static unsigned long to_off(struct header *head, void *p)
{
	return (char *)p - (char *)head;
}

static size_t used_size(unsigned int num_elements)
{
	return align_up(num_elements, BITS_PER_LONG) / CHAR_BIT;
}

/*
 * We always align the first entry to the lower power of 2.
 * eg. the 12-byte bucket gets 8-byte aligned.  The 4096-byte bucket
 * gets 4096-byte aligned.
 */
static unsigned long page_header_size(unsigned int align_bits,
				      unsigned long num_elements)
{
	unsigned long size;

	size = sizeof(struct page_header)
		- sizeof(((struct page_header *)0)->used)
		+ used_size(num_elements);
	return align_up(size, 1 << align_bits);
}

static void add_to_list(struct header *head,
			unsigned long *list, struct page_header *ph)
{
	unsigned long h = *list, offset = to_off(head, ph);

	ph->next = h;
	if (h) {
		struct page_header *prev = from_off(head, h);
		assert(prev->prev == 0);
		prev->prev = offset;
	}
	*list = offset;
	ph->prev = 0;
}

static void del_from_list(struct header *head,
			  unsigned long *list, struct page_header *ph)
{
	/* Front of list? */
	if (ph->prev == 0) {
		*list = ph->next;
	} else {
		struct page_header *prev = from_off(head, ph->prev);
		prev->next = ph->next;
	}
	if (ph->next != 0) {
		struct page_header *next = from_off(head, ph->next);
		next->prev = ph->prev;
	}
}

static unsigned long pop_from_list(struct header *head,
				   unsigned long *list)
{
	unsigned long h = *list;
	struct page_header *ph = from_off(head, h);

	if (likely(h)) {
		*list = ph->next;
		if (*list) {
			struct page_header *next = from_off(head, *list);
			next->prev = 0;
		}
	}
	return h;
}

static void add_small_page_to_freelist(struct header *head,
				       struct page_header *ph)
{
	add_to_list(head, &head->small_free_list, ph);
}

static void add_large_page_to_freelist(struct header *head,
				       struct page_header *ph)
{
	add_to_list(head, &head->large_free_list, ph);
}

static void add_to_bucket_list(struct header *head,
			       struct bucket_state *bs,
			       struct page_header *ph)
{
	add_to_list(head, &bs->page_list, ph);
}

static void del_from_bucket_list(struct header *head,
				 struct bucket_state *bs,
				 struct page_header *ph)
{
	del_from_list(head, &bs->page_list, ph);
}

static void del_from_bucket_full_list(struct header *head,
				      struct bucket_state *bs,
				      struct page_header *ph)
{
	del_from_list(head, &bs->full_list, ph);
}

static void add_to_bucket_full_list(struct header *head,
				    struct bucket_state *bs,
				    struct page_header *ph)
{
	add_to_list(head, &bs->full_list, ph);
}

static void clear_bit(unsigned long bitmap[], unsigned int off)
{
	bitmap[off / BITS_PER_LONG] &= ~(1 << (off % BITS_PER_LONG));
}

static bool test_bit(const unsigned long bitmap[], unsigned int off)
{
	return bitmap[off / BITS_PER_LONG] & (1 << (off % BITS_PER_LONG));
}

static void set_bit(unsigned long bitmap[], unsigned int off)
{
	bitmap[off / BITS_PER_LONG] |= (1 << (off % BITS_PER_LONG));
}

/* There must be a bit to be found. */
static unsigned int find_free_bit(const unsigned long bitmap[])
{
	unsigned int i;

	for (i = 0; bitmap[i] == -1UL; i++);
	return (i*BITS_PER_LONG) + ffsl(~bitmap[i]) - 1;
}

/* How many elements can we fit in a page? */
static unsigned long elements_per_page(unsigned long align_bits,
				       unsigned long esize,
				       unsigned long psize)
{
	unsigned long num, overhead;

	/* First approximation: no extra room for bitmap. */
	overhead = align_up(sizeof(struct page_header), 1 << align_bits);
	num = (psize - overhead) / esize;

	while (page_header_size(align_bits, num) + esize * num > psize)
		num--;
	return num;
}

static bool large_page_bucket(unsigned int bucket, unsigned long poolsize)
{
	unsigned int sp_bits;
	unsigned long max_smallsize;

	sp_bits = large_page_bits(poolsize) - BITS_FROM_SMALL_TO_LARGE_PAGE;
	/* Note: this doesn't take into account page header. */
	max_smallsize = (1UL << sp_bits) >> MAX_PAGE_OBJECT_ORDER;

	return bucket_to_size(bucket) > max_smallsize;
}

static unsigned int max_bucket(unsigned int lp_bits)
{
	return (lp_bits - MAX_PAGE_OBJECT_ORDER) * INTER_BUCKET_SPACE;
}

void alloc_init(void *pool, unsigned long poolsize)
{
	struct header *head = pool;
	struct page_header *ph;
	unsigned int lp_bits, sp_bits, num_buckets;
	unsigned long header_size, i;

	if (poolsize < MIN_USEFUL_SIZE) {
		tiny_alloc_init(pool, poolsize);
		return;
	}

	lp_bits = large_page_bits(poolsize);
	sp_bits = lp_bits - BITS_FROM_SMALL_TO_LARGE_PAGE;

	num_buckets = max_bucket(lp_bits);

	head = pool;
	header_size = sizeof(*head) + sizeof(head->bs) * (num_buckets-1);

	memset(head, 0, header_size);
	for (i = 0; i < num_buckets; i++) {
		unsigned long pagesize;

		if (large_page_bucket(i, poolsize))
			pagesize = 1UL << lp_bits;
		else
			pagesize = 1UL << sp_bits;

		head->bs[i].elements_per_page
			= elements_per_page(i / INTER_BUCKET_SPACE,
					    bucket_to_size(i),
					    pagesize);
	}

	/* They start as all large pages. */
	memset(head->pagesize, 0xFF, sizeof(head->pagesize));
	/* FIXME: small pages for last bit? */

	/* Split first page into small pages. */
	assert(header_size << (1UL << lp_bits));
	clear_bit(head->pagesize, 0);

	/* Skip over page(s) used by header, add rest to free list */
	for (i = align_up(header_size, (1 << sp_bits)) >> sp_bits;
	     i < (1 << BITS_FROM_SMALL_TO_LARGE_PAGE);
	     i++) {
		ph = from_off(head, i<<sp_bits);
		ph->elements_used = 0;
		add_small_page_to_freelist(head, ph);
	}

	/* Add the rest of the pages as large pages. */
	i = (1 << lp_bits);
	while (i + (1 << lp_bits) <= poolsize) {
		ph = from_off(head, i);
		ph->elements_used = 0;
		add_large_page_to_freelist(head, ph);
		i += (1 << lp_bits);
	}
}

/* A large page worth of small pages are free: delete them from free list. */
static void del_large_from_small_free_list(struct header *head,
					   struct page_header *ph,
					   unsigned int sp_bits)
{
	unsigned long i;

	for (i = 0; i < (1 << BITS_FROM_SMALL_TO_LARGE_PAGE); i++) {
		del_from_list(head, &head->small_free_list,
			      (void *)ph + (i << sp_bits));
	}
}

static bool all_empty(struct header *head, unsigned long off, unsigned sp_bits)
{
	unsigned long i;

	for (i = 0; i < (1 << BITS_FROM_SMALL_TO_LARGE_PAGE); i++) {
		struct page_header *ph = from_off(head, off + (i << sp_bits));
		if (ph->elements_used)
			return false;
	}
	return true;
}

static unsigned long get_large_page(struct header *head,
				    unsigned long poolsize)
{
	unsigned long lp_bits, sp_bits, i, page;

	page = pop_from_list(head, &head->large_free_list);
	if (likely(page))
		return page;

	/* Look for small pages to coalesce, after first large page. */
	lp_bits = large_page_bits(poolsize);
	sp_bits = lp_bits - BITS_FROM_SMALL_TO_LARGE_PAGE;

	for (i = (1 << lp_bits); i < poolsize; i += (1 << lp_bits)) {
		/* Already a large page? */
		if (test_bit(head->pagesize, i >> lp_bits))
			continue;
		if (all_empty(head, i, sp_bits)) {
			struct page_header *ph = from_off(head, i);
			set_bit(head->pagesize, i >> lp_bits);
			del_large_from_small_free_list(head, ph, sp_bits);
			add_large_page_to_freelist(head, ph);
		}
	}
			
	return pop_from_list(head, &head->large_free_list);
}

/* Returns small page. */
static unsigned long break_up_large_page(struct header *head,
					 unsigned long psize,
					 unsigned long lpage)
{
	unsigned long lp_bits, sp_bits, i;

	lp_bits = large_page_bits(psize);
	sp_bits = lp_bits - BITS_FROM_SMALL_TO_LARGE_PAGE;
	clear_bit(head->pagesize, lpage >> lp_bits);

	for (i = 1; i < (1 << BITS_FROM_SMALL_TO_LARGE_PAGE); i++)
		add_small_page_to_freelist(head,
					   from_off(head,
						    lpage + (i<<sp_bits)));

	return lpage;
}

static unsigned long get_small_page(struct header *head,
				    unsigned long poolsize)
{
	unsigned long ret;

	ret = pop_from_list(head, &head->small_free_list);
	if (likely(ret))
		return ret;
	ret = get_large_page(head, poolsize);
	if (likely(ret))
		ret = break_up_large_page(head, poolsize, ret);
	return ret;
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	struct header *head = pool;
	unsigned int bucket;
	unsigned long i;
	struct bucket_state *bs;
	struct page_header *ph;

	if (poolsize < MIN_USEFUL_SIZE) {
		return tiny_alloc_get(pool, poolsize, size, align);
	}

	size = align_up(size, align);
	if (unlikely(!size))
		size = 1;
	bucket = size_to_bucket(size);

	if (bucket >= max_bucket(large_page_bits(poolsize))) {
		/* FIXME: huge alloc. */
		return NULL;
	}

	bs = &head->bs[bucket];

	if (!bs->page_list) {
		struct page_header *ph;

		if (large_page_bucket(bucket, poolsize))
			bs->page_list = get_large_page(head, poolsize);
		else
			bs->page_list = get_small_page(head, poolsize);
		/* FIXME: Try large-aligned alloc?  Header stuffing? */
		if (unlikely(!bs->page_list))
			return NULL;
		ph = from_off(head, bs->page_list);
		ph->bucket = bucket;
		ph->elements_used = 0;
		ph->next = 0;
		memset(ph->used, 0, used_size(bs->elements_per_page));
	}

	ph = from_off(head, bs->page_list);

	i = find_free_bit(ph->used);
	set_bit(ph->used, i);
	ph->elements_used++;

	/* check if this page is now full */
	if (unlikely(ph->elements_used == bs->elements_per_page)) {
		del_from_bucket_list(head, bs, ph);
		add_to_bucket_full_list(head, bs, ph);
	}

	return (char *)ph + page_header_size(ph->bucket / INTER_BUCKET_SPACE,
					     bs->elements_per_page)
	       + i * bucket_to_size(bucket);
}

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	struct header *head = pool;
	struct bucket_state *bs;
	unsigned int pagebits;
	unsigned long i, pgoffset, offset = (char *)free - (char *)pool;
	bool smallpage;
	struct page_header *ph;

	if (poolsize < MIN_USEFUL_SIZE) {
		return tiny_alloc_free(pool, poolsize, free);
	}

	/* Get page header. */
	pagebits = large_page_bits(poolsize);
	if (!test_bit(head->pagesize, offset >> pagebits)) {
		smallpage = true;
		pagebits -= BITS_FROM_SMALL_TO_LARGE_PAGE;
	} else
		smallpage = false;

	/* Step back to page header. */
	ph = from_off(head, offset & ~((1UL << pagebits) - 1));
	bs = &head->bs[ph->bucket];
	pgoffset = (offset & ((1UL << pagebits) - 1))
		- page_header_size(ph->bucket / INTER_BUCKET_SPACE,
				   bs->elements_per_page);

	if (unlikely(ph->elements_used == bs->elements_per_page)) {
		del_from_bucket_full_list(head, bs, ph);
		add_to_bucket_list(head, bs, ph);
	}

	/* Which element are we? */
	i = pgoffset / bucket_to_size(ph->bucket);
	clear_bit(ph->used, i);
	ph->elements_used--;

	if (unlikely(ph->elements_used == 0)) {
		bs = &head->bs[ph->bucket];
		del_from_bucket_list(head, bs, ph);
		if (smallpage)
			add_small_page_to_freelist(head, ph);
		else
			add_large_page_to_freelist(head, ph);
	}
}

unsigned long alloc_size(void *pool, unsigned long poolsize, void *p)
{
	struct header *head = pool;
	unsigned int pagebits;
	unsigned long offset = (char *)p - (char *)pool;
	struct page_header *ph;

	if (poolsize < MIN_USEFUL_SIZE)
		return tiny_alloc_size(pool, poolsize, p);

	/* Get page header. */
	pagebits = large_page_bits(poolsize);
	if (!test_bit(head->pagesize, offset >> pagebits))
		pagebits -= BITS_FROM_SMALL_TO_LARGE_PAGE;

	/* Step back to page header. */
	ph = from_off(head, offset & ~((1UL << pagebits) - 1));
	return bucket_to_size(ph->bucket);
}

/* Useful for gdb breakpoints. */
static bool check_fail(void)
{
	return false;
}

static unsigned long count_bits(const unsigned long bitmap[],
				unsigned long limit)
{
	unsigned long i, count = 0;

	while (limit >= BITS_PER_LONG) {
		count += popcount(bitmap[0]);
		bitmap++;
		limit -= BITS_PER_LONG;
	}

	for (i = 0; i < limit; i++)
		if (test_bit(bitmap, i))
			count++;
	return count;
}

static bool out_of_bounds(unsigned long off,
			  unsigned long pagesize,
			  unsigned long poolsize)
{
	return (off > poolsize || off + pagesize > poolsize);
}

static bool check_bucket(struct header *head,
			 unsigned long poolsize,
			 unsigned long pages[],
			 struct bucket_state *bs,
			 unsigned int bindex)
{
	bool lp_bucket = large_page_bucket(bindex, poolsize);
	struct page_header *ph;
	unsigned long taken, i, prev, pagesize, sp_bits, lp_bits;

	lp_bits = large_page_bits(poolsize);
	sp_bits = lp_bits - BITS_FROM_SMALL_TO_LARGE_PAGE;

	pagesize = 1UL << (lp_bucket ? lp_bits : sp_bits);

	/* This many elements fit? */
	taken = page_header_size(bindex / INTER_BUCKET_SPACE,
				 bs->elements_per_page);
	taken += bucket_to_size(bindex) * bs->elements_per_page;
	if (taken > pagesize)
		return check_fail();

	/* One more wouldn't fit? */
	taken = page_header_size(bindex / INTER_BUCKET_SPACE,
				 bs->elements_per_page + 1);
	taken += bucket_to_size(bindex) * (bs->elements_per_page + 1);
	if (taken <= pagesize)
		return check_fail();

	/* Walk used list. */
	prev = 0;
	for (i = bs->page_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, pagesize, poolsize))
			return check_fail();
		/* Wrong size page? */
		if (!!test_bit(head->pagesize, i >> lp_bits) != lp_bucket)
			return check_fail();
		/* Not page boundary? */
		if (i % pagesize)
			return check_fail();
		ph = from_off(head, i);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i >> sp_bits))
			return check_fail();
		set_bit(pages, i >> sp_bits);
		/* Empty or full? */
		if (ph->elements_used == 0)
			return check_fail();
		if (ph->elements_used >= bs->elements_per_page)
			return check_fail();
		/* Used bits don't agree? */
		if (ph->elements_used != count_bits(ph->used,
						    bs->elements_per_page))
			return check_fail();
		/* Wrong bucket? */
		if (ph->bucket != bindex)
			return check_fail();
		prev = i;
	}

	/* Walk full list. */
	prev = 0;
	for (i = bs->full_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, pagesize, poolsize))
			return check_fail();
		/* Wrong size page? */
		if (!!test_bit(head->pagesize, i >> lp_bits) != lp_bucket)
			return check_fail();
		/* Not page boundary? */
		if (i % pagesize)
			return check_fail();
		ph = from_off(head, i);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i >> sp_bits))
			return check_fail();
		set_bit(pages, i >> sp_bits);
		/* Not full? */
		if (ph->elements_used != bs->elements_per_page)
			return check_fail();
		/* Used bits don't agree? */
		if (ph->elements_used != count_bits(ph->used,
						    bs->elements_per_page))
			return check_fail();
		/* Wrong bucket? */
		if (ph->bucket != bindex)
			return check_fail();
		prev = i;
	}
	return true;
}

bool alloc_check(void *pool, unsigned long poolsize)
{
	struct header *head = pool;
	unsigned long prev, i, lp_bits, sp_bits, header_size, num_buckets;
	struct page_header *ph;
	unsigned long pages[(MAX_PAGES << BITS_FROM_SMALL_TO_LARGE_PAGE)
			    / BITS_PER_LONG] = { 0 };

	if (poolsize < MIN_USEFUL_SIZE)
		return tiny_alloc_check(pool, poolsize);

	lp_bits = large_page_bits(poolsize);
	sp_bits = lp_bits - BITS_FROM_SMALL_TO_LARGE_PAGE;

	num_buckets = max_bucket(lp_bits);

	header_size = sizeof(*head) + sizeof(head->bs) * (num_buckets-1);

	/* First, set all bits taken by header. */
	for (i = 0; i < header_size; i += (1UL << sp_bits))
		set_bit(pages, i >> sp_bits);

	/* Check small page free list. */
	prev = 0;
	for (i = head->small_free_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, 1 << sp_bits, poolsize))
			return check_fail();
		/* Large page? */
		if (test_bit(head->pagesize, i >> lp_bits))
			return check_fail();
		/* Not page boundary? */
		if (i % (1 << sp_bits))
			return check_fail();
		ph = from_off(head, i);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i >> sp_bits))
			return check_fail();
		set_bit(pages, i >> sp_bits);
		prev = i;
	}

	/* Check large page free list. */
	prev = 0;
	for (i = head->large_free_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, 1 << lp_bits, poolsize))
			return check_fail();
		/* Not large page? */
		if (!test_bit(head->pagesize, i >> lp_bits))
			return check_fail();
		/* Not page boundary? */
		if (i % (1 << lp_bits))
			return check_fail();
		ph = from_off(head, i);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i >> sp_bits))
			return check_fail();
		set_bit(pages, i >> sp_bits);
		prev = i;
	}

	/* Check the buckets. */
	for (i = 0; i < max_bucket(lp_bits); i++) {
		struct bucket_state *bs = &head->bs[i];

		if (!check_bucket(head, poolsize, pages, bs, i))
			return false;
	}

	/* Make sure every page accounted for. */
	for (i = 0; i < poolsize >> sp_bits; i++) {
		if (!test_bit(pages, i))
			return check_fail();
		if (test_bit(head->pagesize,
			     i >> BITS_FROM_SMALL_TO_LARGE_PAGE)) {
			/* Large page, skip rest. */
			i += (1 << BITS_FROM_SMALL_TO_LARGE_PAGE) - 1;
		}
	}

	return true;
}
