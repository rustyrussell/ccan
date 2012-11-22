/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include "alloc.h"
#include "bitops.h"
#include "tiny.h"
#include <ccan/build_assert/build_assert.h>
#include <ccan/likely/likely.h>
#include <ccan/alignof/alignof.h>
#include <ccan/short_types/short_types.h>
#include <ccan/compiler/compiler.h>
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

/* We divide the pool into this many large pages (nearest power of 2) */
#define MAX_LARGE_PAGES (256UL)

/* 32 small pages == 1 large page. */
#define BITS_FROM_SMALL_TO_LARGE_PAGE 5

#define MAX_SMALL_PAGES (MAX_LARGE_PAGES << BITS_FROM_SMALL_TO_LARGE_PAGE)

/* Smallest pool size for this scheme: 128-byte small pages.  That's
 * 9/13% overhead for 32/64 bit. */
#define MIN_USEFUL_SIZE (MAX_SMALL_PAGES * 128)

/* Every 4 buckets, we jump up a power of 2. ...8 10 12 14 16 20 24 28 32... */
#define INTER_BUCKET_SPACE 4

#define SMALL_PAGES_PER_LARGE_PAGE (1 << BITS_FROM_SMALL_TO_LARGE_PAGE)

/* FIXME: Figure this out properly. */
#define MAX_SIZE (1 << 30)

/* How few object to fit in a page before using a larger one? (8) */
#define MAX_PAGE_OBJECT_ORDER	3

#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)

struct bucket_state {
	u32 elements_per_page;
	u16 page_list;
	u16 full_list;
};

struct header {
	/* Bitmap of which pages are large. */
	unsigned long pagesize[MAX_LARGE_PAGES / BITS_PER_LONG];

	/* List of unused small/large pages. */
	u16 small_free_list;
	u16 large_free_list;

	/* List of huge allocs. */
	unsigned long huge;

	/* This is less defined: we have two buckets for each power of 2 */
	struct bucket_state bs[1];
};

struct huge_alloc {
	unsigned long next, prev;
	unsigned long off, len;
};

struct page_header {
	u16 next, prev;
	/* FIXME: We can just count all-0 and all-1 used[] elements. */
	unsigned elements_used : 25;
	unsigned bucket : 7;
	unsigned long used[1]; /* One bit per element. */
};

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
	unsigned long base = 1UL << (bucket / INTER_BUCKET_SPACE);
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
	unsigned int base = afls(size/2);
	unsigned long overshoot;

	overshoot = size - (1UL << base);
	return base * INTER_BUCKET_SPACE
		+ ((overshoot * INTER_BUCKET_SPACE + (1UL << base)-1) >> base);
}

static unsigned int small_page_bits(unsigned long poolsize)
{
	return afls(poolsize / MAX_SMALL_PAGES - 1);
}

static struct page_header *from_pgnum(struct header *head,
				      unsigned long pgnum,
				      unsigned sp_bits)
{
	return (struct page_header *)((char *)head + (pgnum << sp_bits));
}

static u16 to_pgnum(struct header *head, void *p, unsigned sp_bits)
{
	return ((char *)p - (char *)head) >> sp_bits;
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
	return align_up(size, 1UL << align_bits);
}

static void add_to_list(struct header *head,
			u16 *list, struct page_header *ph, unsigned sp_bits)
{
	unsigned long h = *list, offset = to_pgnum(head, ph, sp_bits);

	ph->next = h;
	if (h) {
		struct page_header *prev = from_pgnum(head, h, sp_bits);
		assert(prev->prev == 0);
		prev->prev = offset;
	}
	*list = offset;
	ph->prev = 0;
}

static void del_from_list(struct header *head,
			  u16 *list, struct page_header *ph, unsigned sp_bits)
{
	/* Front of list? */
	if (ph->prev == 0) {
		*list = ph->next;
	} else {
		struct page_header *prev = from_pgnum(head, ph->prev, sp_bits);
		prev->next = ph->next;
	}
	if (ph->next != 0) {
		struct page_header *next = from_pgnum(head, ph->next, sp_bits);
		next->prev = ph->prev;
	}
}

static u16 pop_from_list(struct header *head,
				   u16 *list,
				   unsigned int sp_bits)
{
	u16 h = *list;
	struct page_header *ph = from_pgnum(head, h, sp_bits);

	if (likely(h)) {
		*list = ph->next;
		if (*list)
			from_pgnum(head, *list, sp_bits)->prev = 0;
	}
	return h;
}

static void add_to_huge_list(struct header *head, struct huge_alloc *ha)
{
	unsigned long h = head->huge;
	unsigned long offset = (char *)ha - (char *)head;

	ha->next = h;
	if (h) {
		struct huge_alloc *prev = (void *)((char *)head + h);
		assert(prev->prev == 0);
		prev->prev = offset;
	}
	head->huge = offset;
	ha->prev = 0;
}

static void del_from_huge(struct header *head, struct huge_alloc *ha)
{
	/* Front of list? */
	if (ha->prev == 0) {
		head->huge = ha->next;
	} else {
		struct huge_alloc *prev = (void *)((char *)head + ha->prev);
		prev->next = ha->next;
	}
	if (ha->next != 0) {
		struct huge_alloc *next = (void *)((char *)head + ha->next);
		next->prev = ha->prev;
	}
}

static void add_small_page_to_freelist(struct header *head,
				       struct page_header *ph,
				       unsigned int sp_bits)
{
	add_to_list(head, &head->small_free_list, ph, sp_bits);
}

static void add_large_page_to_freelist(struct header *head,
				       struct page_header *ph,
				       unsigned int sp_bits)
{
	add_to_list(head, &head->large_free_list, ph, sp_bits);
}

static void add_to_bucket_list(struct header *head,
			       struct bucket_state *bs,
			       struct page_header *ph,
			       unsigned int sp_bits)
{
	add_to_list(head, &bs->page_list, ph, sp_bits);
}

static void del_from_bucket_list(struct header *head,
				 struct bucket_state *bs,
				 struct page_header *ph,
				 unsigned int sp_bits)
{
	del_from_list(head, &bs->page_list, ph, sp_bits);
}

static void del_from_bucket_full_list(struct header *head,
				      struct bucket_state *bs,
				      struct page_header *ph,
				      unsigned int sp_bits)
{
	del_from_list(head, &bs->full_list, ph, sp_bits);
}

static void add_to_bucket_full_list(struct header *head,
				    struct bucket_state *bs,
				    struct page_header *ph,
				    unsigned int sp_bits)
{
	add_to_list(head, &bs->full_list, ph, sp_bits);
}

static void clear_bit(unsigned long bitmap[], unsigned int off)
{
	bitmap[off / BITS_PER_LONG] &= ~(1UL << (off % BITS_PER_LONG));
}

static bool test_bit(const unsigned long bitmap[], unsigned int off)
{
	return bitmap[off / BITS_PER_LONG] & (1UL << (off % BITS_PER_LONG));
}

static void set_bit(unsigned long bitmap[], unsigned int off)
{
	bitmap[off / BITS_PER_LONG] |= (1UL << (off % BITS_PER_LONG));
}

/* There must be a bit to be found. */
static unsigned int find_free_bit(const unsigned long bitmap[])
{
	unsigned int i;

	for (i = 0; bitmap[i] == -1UL; i++);
	return (i*BITS_PER_LONG) + affsl(~bitmap[i]) - 1;
}

/* How many elements can we fit in a page? */
static unsigned long elements_per_page(unsigned long align_bits,
				       unsigned long esize,
				       unsigned long psize)
{
	unsigned long num, overhead;

	/* First approximation: no extra room for bitmap. */
	overhead = align_up(sizeof(struct page_header), 1UL << align_bits);
	num = (psize - overhead) / esize;

	while (page_header_size(align_bits, num) + esize * num > psize)
		num--;
	return num;
}

static bool large_page_bucket(unsigned int bucket, unsigned int sp_bits)
{
	unsigned long max_smallsize;

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

	/* We rely on page numbers fitting in 16 bit. */
	BUILD_ASSERT(MAX_SMALL_PAGES < 65536);

	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	num_buckets = max_bucket(lp_bits);

	head = pool;
	header_size = sizeof(*head) + sizeof(head->bs) * (num_buckets-1);

	memset(head, 0, header_size);
	for (i = 0; i < num_buckets; i++) {
		unsigned long pagesize;

		if (large_page_bucket(i, sp_bits))
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
	assert(header_size < (1UL << lp_bits));
	clear_bit(head->pagesize, 0);

	/* Skip over page(s) used by header, add rest to free list */
	for (i = align_up(header_size, (1UL << sp_bits)) >> sp_bits;
	     i < SMALL_PAGES_PER_LARGE_PAGE;
	     i++) {
		ph = from_pgnum(head, i, sp_bits);
		ph->elements_used = 0;
		add_small_page_to_freelist(head, ph, sp_bits);
	}

	/* Add the rest of the pages as large pages. */
	i = SMALL_PAGES_PER_LARGE_PAGE;
	while ((i << sp_bits) + (1UL << lp_bits) <= poolsize) {
		assert(i < MAX_SMALL_PAGES);
		ph = from_pgnum(head, i, sp_bits);
		ph->elements_used = 0;
		add_large_page_to_freelist(head, ph, sp_bits);
		i += SMALL_PAGES_PER_LARGE_PAGE;
	}
}

/* A large page worth of small pages are free: delete them from free list. */
static void del_large_from_small_free_list(struct header *head,
					   struct page_header *ph,
					   unsigned int sp_bits)
{
	unsigned long i;

	for (i = 0; i < SMALL_PAGES_PER_LARGE_PAGE; i++) {
		del_from_list(head, &head->small_free_list,
			      (struct page_header *)((char *)ph
						     + (i << sp_bits)),
			      sp_bits);
	}
}

static bool all_empty(struct header *head,
		      unsigned long pgnum,
		      unsigned sp_bits)
{
	unsigned long i;

	for (i = 0; i < SMALL_PAGES_PER_LARGE_PAGE; i++) {
		struct page_header *ph = from_pgnum(head, pgnum + i, sp_bits);
		if (ph->elements_used)
			return false;
	}
	return true;
}

static void recombine_small_pages(struct header *head, unsigned long poolsize,
				  unsigned int sp_bits)
{
	unsigned long i;
	unsigned int lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	/* Look for small pages to coalesce, after first large page. */
	for (i = SMALL_PAGES_PER_LARGE_PAGE;
	     i < (poolsize >> lp_bits) << BITS_FROM_SMALL_TO_LARGE_PAGE;
	     i += SMALL_PAGES_PER_LARGE_PAGE) {
		/* Already a large page? */
		if (test_bit(head->pagesize, i / SMALL_PAGES_PER_LARGE_PAGE))
			continue;
		if (all_empty(head, i, sp_bits)) {
			struct page_header *ph = from_pgnum(head, i, sp_bits);
			set_bit(head->pagesize,
				i / SMALL_PAGES_PER_LARGE_PAGE);
			del_large_from_small_free_list(head, ph, sp_bits);
			add_large_page_to_freelist(head, ph, sp_bits);
		}
	}
}

static u16 get_large_page(struct header *head, unsigned long poolsize,
			  unsigned int sp_bits)
{
	unsigned int page;

	page = pop_from_list(head, &head->large_free_list, sp_bits);
	if (likely(page))
		return page;

	recombine_small_pages(head, poolsize, sp_bits);

	return pop_from_list(head, &head->large_free_list, sp_bits);
}

/* Returns small page. */
static unsigned long break_up_large_page(struct header *head,
					 unsigned int sp_bits,
					 u16 lpage)
{
	unsigned int i;

	clear_bit(head->pagesize, lpage >> BITS_FROM_SMALL_TO_LARGE_PAGE);

	for (i = 1; i < SMALL_PAGES_PER_LARGE_PAGE; i++) {
		struct page_header *ph = from_pgnum(head, lpage + i, sp_bits);
		/* Initialize this: huge_alloc reads it. */
		ph->elements_used = 0;
		add_small_page_to_freelist(head, ph, sp_bits);
	}

	return lpage;
}

static u16 get_small_page(struct header *head, unsigned long poolsize,
			  unsigned int sp_bits)
{
	u16 ret;

	ret = pop_from_list(head, &head->small_free_list, sp_bits);
	if (likely(ret))
		return ret;
	ret = get_large_page(head, poolsize, sp_bits);
	if (likely(ret))
		ret = break_up_large_page(head, sp_bits, ret);
	return ret;
}

static bool huge_allocated(struct header *head, unsigned long offset)
{
	unsigned long i;
	struct huge_alloc *ha;

	for (i = head->huge; i; i = ha->next) {
		ha = (void *)((char *)head + i);
		if (ha->off <= offset && ha->off + ha->len > offset)
			return true;
	}
	return false;
}

/* They want something really big.  Aim for contiguous pages (slow). */
static COLD void *huge_alloc(void *pool, unsigned long poolsize,
			     unsigned long size, unsigned long align)
{
	struct header *head = pool;
	struct huge_alloc *ha;
	unsigned long i, sp_bits, lp_bits, num, header_size;

	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	/* Allocate tracking structure optimistically. */
	ha = alloc_get(pool, poolsize, sizeof(*ha), ALIGNOF(*ha));
	if (!ha)
		return NULL;

	/* First search for contiguous small pages... */
	header_size = sizeof(*head) + sizeof(head->bs) * (max_bucket(lp_bits)-1);

	num = 0;
	for (i = (header_size + (1UL << sp_bits) - 1) >> sp_bits;
	     i << sp_bits < poolsize;
	     i++) {
		struct page_header *pg;
		unsigned long off = (i << sp_bits);

		/* Skip over large pages. */
		if (test_bit(head->pagesize, i >> BITS_FROM_SMALL_TO_LARGE_PAGE)) {
			i += (1UL << BITS_FROM_SMALL_TO_LARGE_PAGE)-1;
			continue;
		}

		/* Does this page meet alignment requirements? */
		if (!num && off % align != 0)
			continue;

		/* FIXME: This makes us O(n^2). */
		if (huge_allocated(head, off)) {
			num = 0;
			continue;
		}

		pg = (struct page_header *)((char *)head + off);
		if (pg->elements_used) {
			num = 0;
			continue;
		}

		num++;
		if (num << sp_bits >= size) {
			unsigned long pgnum;

			/* Remove from free list. */
			for (pgnum = i; pgnum > i - num; pgnum--) {
				pg = from_pgnum(head, pgnum, sp_bits);
				del_from_list(head,
					      &head->small_free_list,
					      pg, sp_bits);
			}
			ha->off = (i - num + 1) << sp_bits;
			ha->len = num << sp_bits;
			goto done;
		}
	}

	/* Now search for large pages... */
	recombine_small_pages(head, poolsize, sp_bits);

	num = 0;
	for (i = (header_size + (1UL << lp_bits) - 1) >> lp_bits;
	     (i << lp_bits) < poolsize; i++) {
		struct page_header *pg;
		unsigned long off = (i << lp_bits);

		/* Ignore small pages. */
		if (!test_bit(head->pagesize, i))
			continue;

		/* Does this page meet alignment requirements? */
		if (!num && off % align != 0)
			continue;

		/* FIXME: This makes us O(n^2). */
		if (huge_allocated(head, off)) {
			num = 0;
			continue;
		}

		pg = (struct page_header *)((char *)head + off);
		if (pg->elements_used) {
			num = 0;
			continue;
		}

		num++;
		if (num << lp_bits >= size) {
			unsigned long pgnum;

			/* Remove from free list. */
			for (pgnum = i; pgnum > i - num; pgnum--) {
				pg = from_pgnum(head, pgnum, lp_bits);
				del_from_list(head,
					      &head->large_free_list,
					      pg, sp_bits);
			}
			ha->off = (i - num + 1) << lp_bits;
			ha->len = num << lp_bits;
			goto done;
		}
	}

	/* Unable to satisfy: free huge alloc structure. */
	alloc_free(pool, poolsize, ha);
	return NULL;

done:
	add_to_huge_list(pool, ha);
	return (char *)pool + ha->off;
}

static COLD void
huge_free(struct header *head, unsigned long poolsize, void *free)
{
	unsigned long i, off, pgnum, free_off = (char *)free - (char *)head;
	unsigned int sp_bits, lp_bits;
	struct huge_alloc *ha;

	for (i = head->huge; i; i = ha->next) {
		ha = (void *)((char *)head + i);
		if (free_off == ha->off)
			break;
	}
	assert(i);

	/* Free up all the pages, delete and free ha */
	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;
	pgnum = free_off >> sp_bits;

	if (test_bit(head->pagesize, pgnum >> BITS_FROM_SMALL_TO_LARGE_PAGE)) {
		for (off = ha->off;
		     off < ha->off + ha->len;
		     off += 1UL << lp_bits) {
			add_large_page_to_freelist(head,
						   (void *)((char *)head + off),
						   sp_bits);
		}
	} else {
		for (off = ha->off;
		     off < ha->off + ha->len;
		     off += 1UL << sp_bits) {
			add_small_page_to_freelist(head,
						   (void *)((char *)head + off),
						   sp_bits);
		}
	}
	del_from_huge(head, ha);
	alloc_free(head, poolsize, ha);
}

static COLD unsigned long huge_size(struct header *head, void *p)
{
	unsigned long i, off = (char *)p - (char *)head;
	struct huge_alloc *ha;

	for (i = head->huge; i; i = ha->next) {
		ha = (void *)((char *)head + i);
		if (off == ha->off) {
			return ha->len;
		}
	}
	abort();
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	struct header *head = pool;
	unsigned int bucket;
	unsigned long i;
	struct bucket_state *bs;
	struct page_header *ph;
	unsigned int sp_bits;

	if (poolsize < MIN_USEFUL_SIZE) {
		return tiny_alloc_get(pool, poolsize, size, align);
	}

	size = align_up(size, align);
	if (unlikely(!size))
		size = 1;
	bucket = size_to_bucket(size);

	sp_bits = small_page_bits(poolsize);

	if (bucket >= max_bucket(sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE)) {
		return huge_alloc(pool, poolsize, size, align);
	}

	bs = &head->bs[bucket];

	if (!bs->page_list) {
		struct page_header *ph;

		if (large_page_bucket(bucket, sp_bits))
			bs->page_list = get_large_page(head, poolsize,
						       sp_bits);
		else
			bs->page_list = get_small_page(head, poolsize,
						       sp_bits);
		/* FIXME: Try large-aligned alloc?  Header stuffing? */
		if (unlikely(!bs->page_list))
			return NULL;
		ph = from_pgnum(head, bs->page_list, sp_bits);
		ph->bucket = bucket;
		ph->elements_used = 0;
		ph->next = 0;
		memset(ph->used, 0, used_size(bs->elements_per_page));
	}

	ph = from_pgnum(head, bs->page_list, sp_bits);

	i = find_free_bit(ph->used);
	set_bit(ph->used, i);
	ph->elements_used++;

	/* check if this page is now full */
	if (unlikely(ph->elements_used == bs->elements_per_page)) {
		del_from_bucket_list(head, bs, ph, sp_bits);
		add_to_bucket_full_list(head, bs, ph, sp_bits);
	}

	return (char *)ph + page_header_size(ph->bucket / INTER_BUCKET_SPACE,
					     bs->elements_per_page)
	       + i * bucket_to_size(bucket);
}

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	struct header *head = pool;
	struct bucket_state *bs;
	unsigned int sp_bits;
	unsigned long i, pgnum, pgoffset, offset = (char *)free - (char *)pool;
	bool smallpage;
	struct page_header *ph;

	if (poolsize < MIN_USEFUL_SIZE) {
		tiny_alloc_free(pool, poolsize, free);
		return;
	}

	/* Get page header. */
	sp_bits = small_page_bits(poolsize);
	pgnum = offset >> sp_bits;

	/* Big page? Round down further. */
	if (test_bit(head->pagesize, pgnum >> BITS_FROM_SMALL_TO_LARGE_PAGE)) {
		smallpage = false;
		pgnum &= ~(SMALL_PAGES_PER_LARGE_PAGE - 1);
	} else
		smallpage = true;

	/* Step back to page header. */
	ph = from_pgnum(head, pgnum, sp_bits);
	if ((void *)ph == free) {
		huge_free(head, poolsize, free);
		return;
	}

	bs = &head->bs[ph->bucket];
	pgoffset = offset - (pgnum << sp_bits)
		- page_header_size(ph->bucket / INTER_BUCKET_SPACE,
				   bs->elements_per_page);

	if (unlikely(ph->elements_used == bs->elements_per_page)) {
		del_from_bucket_full_list(head, bs, ph, sp_bits);
		add_to_bucket_list(head, bs, ph, sp_bits);
	}

	/* Which element are we? */
	i = pgoffset / bucket_to_size(ph->bucket);
	clear_bit(ph->used, i);
	ph->elements_used--;

	if (unlikely(ph->elements_used == 0)) {
		bs = &head->bs[ph->bucket];
		del_from_bucket_list(head, bs, ph, sp_bits);
		if (smallpage)
			add_small_page_to_freelist(head, ph, sp_bits);
		else
			add_large_page_to_freelist(head, ph, sp_bits);
	}
}

unsigned long alloc_size(void *pool, unsigned long poolsize, void *p)
{
	struct header *head = pool;
	unsigned int pgnum, sp_bits;
	unsigned long offset = (char *)p - (char *)pool;
	struct page_header *ph;

	if (poolsize < MIN_USEFUL_SIZE)
		return tiny_alloc_size(pool, poolsize, p);

	/* Get page header. */
	sp_bits = small_page_bits(poolsize);
	pgnum = offset >> sp_bits;

	/* Big page? Round down further. */
	if (test_bit(head->pagesize, pgnum >> BITS_FROM_SMALL_TO_LARGE_PAGE))
		pgnum &= ~(SMALL_PAGES_PER_LARGE_PAGE - 1);

	/* Step back to page header. */
	ph = from_pgnum(head, pgnum, sp_bits);
	if ((void *)ph == p)
		return huge_size(head, p);

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

static bool out_of_bounds(unsigned long pgnum,
			  unsigned int sp_bits,
			  unsigned long pagesize,
			  unsigned long poolsize)
{
	if (((pgnum << sp_bits) >> sp_bits) != pgnum)
		return true;

	if ((pgnum << sp_bits) > poolsize)
		return true;

	return ((pgnum << sp_bits) + pagesize > poolsize);
}

static bool check_bucket(struct header *head,
			 unsigned long poolsize,
			 unsigned long pages[],
			 struct bucket_state *bs,
			 unsigned int bindex)
{
	bool lp_bucket;
	struct page_header *ph;
	unsigned long taken, i, prev, pagesize, sp_bits, lp_bits;

	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	lp_bucket = large_page_bucket(bindex, sp_bits);

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
		if (out_of_bounds(i, sp_bits, pagesize, poolsize))
			return check_fail();
		/* Wrong size page? */
		if (!!test_bit(head->pagesize, i >> BITS_FROM_SMALL_TO_LARGE_PAGE)
		    != lp_bucket)
			return check_fail();
		/* Large page not on boundary? */
		if (lp_bucket && (i % SMALL_PAGES_PER_LARGE_PAGE) != 0)
			return check_fail();
		ph = from_pgnum(head, i, sp_bits);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i))
			return check_fail();
		set_bit(pages, i);
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
		if (out_of_bounds(i, sp_bits, pagesize, poolsize))
			return check_fail();
		/* Wrong size page? */
		if (!!test_bit(head->pagesize, i >> BITS_FROM_SMALL_TO_LARGE_PAGE)
		    != lp_bucket)
		/* Large page not on boundary? */
		if (lp_bucket && (i % SMALL_PAGES_PER_LARGE_PAGE) != 0)
			return check_fail();
		ph = from_pgnum(head, i, sp_bits);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i))
			return check_fail();
		set_bit(pages, i);
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
	struct huge_alloc *ha;
	unsigned long pages[MAX_SMALL_PAGES / BITS_PER_LONG] = { 0 };

	if (poolsize < MIN_USEFUL_SIZE)
		return tiny_alloc_check(pool, poolsize);

	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	num_buckets = max_bucket(lp_bits);

	header_size = sizeof(*head) + sizeof(head->bs) * (num_buckets-1);

	/* First, set all bits taken by header. */
	for (i = 0; i < header_size; i += (1UL << sp_bits))
		set_bit(pages, i >> sp_bits);

	/* Check small page free list. */
	prev = 0;
	for (i = head->small_free_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, sp_bits, 1UL << sp_bits, poolsize))
			return check_fail();
		/* Large page? */
		if (test_bit(head->pagesize, i >> BITS_FROM_SMALL_TO_LARGE_PAGE))
			return check_fail();
		ph = from_pgnum(head, i, sp_bits);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i))
			return check_fail();
		set_bit(pages, i);
		prev = i;
	}

	/* Check large page free list. */
	prev = 0;
	for (i = head->large_free_list; i; i = ph->next) {
		/* Bad pointer? */
		if (out_of_bounds(i, sp_bits, 1UL << lp_bits, poolsize))
			return check_fail();
		/* Not large page? */
		if (!test_bit(head->pagesize, i >> BITS_FROM_SMALL_TO_LARGE_PAGE))
			return check_fail();
		/* Not page boundary? */
		if ((i % SMALL_PAGES_PER_LARGE_PAGE) != 0)
			return check_fail();
		ph = from_pgnum(head, i, sp_bits);
		/* Linked list corrupt? */
		if (ph->prev != prev)
			return check_fail();
		/* Already seen this page? */
		if (test_bit(pages, i))
			return check_fail();
		set_bit(pages, i);
		prev = i;
	}

	/* Check the buckets. */
	for (i = 0; i < max_bucket(lp_bits); i++) {
		struct bucket_state *bs = &head->bs[i];

		if (!check_bucket(head, poolsize, pages, bs, i))
			return false;
	}

	/* Check the huge alloc list. */
	prev = 0;
	for (i = head->huge; i; i = ha->next) {
		unsigned long pgbits, j;

		/* Bad pointer? */
		if (i >= poolsize || i + sizeof(*ha) > poolsize)
			return check_fail();
		ha = (void *)((char *)head + i);

		/* Check contents of ha. */
		if (ha->off > poolsize || ha->off + ha->len > poolsize)
			return check_fail();

		/* Large or small page? */
		pgbits = test_bit(head->pagesize, ha->off >> lp_bits)
			? lp_bits : sp_bits;

		/* Not page boundary? */
		if ((ha->off % (1UL << pgbits)) != 0)
			return check_fail();

		/* Not page length? */
		if ((ha->len % (1UL << pgbits)) != 0)
			return check_fail();

		/* Linked list corrupt? */
		if (ha->prev != prev)
			return check_fail();

		for (j = ha->off; j < ha->off + ha->len; j += (1UL<<sp_bits)) {
			/* Already seen this page? */
			if (test_bit(pages, j >> sp_bits))
				return check_fail();
			set_bit(pages, j >> sp_bits);
		}

		prev = i;
	}

	/* Make sure every page accounted for. */
	for (i = 0; i < poolsize >> sp_bits; i++) {
		if (!test_bit(pages, i))
			return check_fail();
		if (test_bit(head->pagesize,
			     i >> BITS_FROM_SMALL_TO_LARGE_PAGE)) {
			/* Large page, skip rest. */
			i += SMALL_PAGES_PER_LARGE_PAGE - 1;
		}
	}

	return true;
}

static unsigned long print_overhead(FILE *out, const char *desc,
				    unsigned long bytes,
				    unsigned long poolsize)
{
	fprintf(out, "Overhead (%s): %lu bytes (%.3g%%)\n",
		desc, bytes, 100.0 * bytes / poolsize);
	return bytes;
}

static unsigned long count_list(struct header *head,
				u16 pgnum,
				unsigned int sp_bits,
				unsigned long *total_elems)
{
	struct page_header *p;
	unsigned long ret = 0;

	while (pgnum) {
		p = from_pgnum(head, pgnum, sp_bits);
		if (total_elems)
			(*total_elems) += p->elements_used;
		ret++;
		pgnum = p->next;
	}
	return ret;
}

static unsigned long visualize_bucket(FILE *out, struct header *head,
				      unsigned int bucket,
				      unsigned long poolsize,
				      unsigned int sp_bits)
{
	unsigned long num_full, num_partial, num_pages, page_size,
		elems, hdr_min, hdr_size, elems_per_page, overhead = 0;

	elems_per_page = head->bs[bucket].elements_per_page;

	/* If we used byte-based bitmaps, we could get pg hdr to: */
	hdr_min = sizeof(struct page_header)
		- sizeof(((struct page_header *)0)->used)
		+ align_up(elems_per_page, CHAR_BIT) / CHAR_BIT;
	hdr_size = page_header_size(bucket / INTER_BUCKET_SPACE,
				    elems_per_page);

	elems = 0;
	num_full = count_list(head, head->bs[bucket].full_list, sp_bits,
			      &elems);
	num_partial = count_list(head, head->bs[bucket].page_list, sp_bits,
				 &elems);
	num_pages = num_full + num_partial;
	if (!num_pages)
		return 0;

	fprintf(out, "Bucket %u (%lu bytes):"
		" %lu full, %lu partial = %lu elements\n",
		bucket, bucket_to_size(bucket), num_full, num_partial, elems);
	/* Strict requirement of page header size. */
	overhead += print_overhead(out, "page headers",
				   hdr_min * num_pages, poolsize);
	/* Gap between minimal page header and actual start. */
	overhead += print_overhead(out, "page post-header alignments",
				   (hdr_size - hdr_min) * num_pages, poolsize);
	/* Between last element and end of page. */
	page_size = (1UL << sp_bits);
	if (large_page_bucket(bucket, sp_bits))
		page_size <<= BITS_FROM_SMALL_TO_LARGE_PAGE;

	overhead += print_overhead(out, "page tails",
				   (page_size - (hdr_size
						 + (elems_per_page
						    * bucket_to_size(bucket))))
				   * num_pages, poolsize);
	return overhead;
}

void alloc_visualize(FILE *out, void *pool, unsigned long poolsize)
{
	struct header *head = pool;
	unsigned long i, lp_bits, sp_bits, header_size, num_buckets, count,
		overhead = 0;

	fprintf(out, "Pool %p size %lu: (%s allocator)\n", pool, poolsize,
		poolsize < MIN_USEFUL_SIZE ? "tiny" : "standard");

	if (poolsize < MIN_USEFUL_SIZE) {
		tiny_alloc_visualize(out, pool, poolsize);
		return;
	}

	sp_bits = small_page_bits(poolsize);
	lp_bits = sp_bits + BITS_FROM_SMALL_TO_LARGE_PAGE;

	num_buckets = max_bucket(lp_bits);
	header_size = sizeof(*head) + sizeof(head->bs) * (num_buckets-1);

	fprintf(out, "Large page size %lu, small page size %lu.\n",
		1UL << lp_bits, 1UL << sp_bits);
	overhead += print_overhead(out, "unused pool tail",
				   poolsize % (1UL << lp_bits), poolsize);
	fprintf(out, "Main header %lu bytes (%lu small pages).\n",
		header_size, align_up(header_size, 1UL << sp_bits) >> sp_bits);
	overhead += print_overhead(out, "partial header page",
				   align_up(header_size, 1UL << sp_bits)
				   - header_size, poolsize);
	/* Total large pages. */
	i = count_bits(head->pagesize, poolsize >> lp_bits);
	/* Used pages. */
	count = i - count_list(head, head->large_free_list, sp_bits, NULL);
	fprintf(out, "%lu/%lu large pages used (%.3g%%)\n",
		count, i, count ? 100.0 * count / i : 0.0);

	/* Total small pages. */
	i = ((poolsize >> lp_bits) - i) << BITS_FROM_SMALL_TO_LARGE_PAGE;
	/* Used pages */
	count = i - count_list(head, head->small_free_list, sp_bits, NULL);
	fprintf(out, "%lu/%lu small pages used (%.3g%%)\n",
		count, i, count ? 100.0 * count / i : 0.0);

	/* Summary of each bucket. */
	fprintf(out, "%lu buckets:\n", num_buckets);
	for (i = 0; i < num_buckets; i++)
		overhead += visualize_bucket(out, head, i, poolsize, sp_bits);

	print_overhead(out, "total", overhead, poolsize);
}
