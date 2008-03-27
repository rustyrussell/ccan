#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "alloc.h"
#include "build_assert/build_assert.h"
#include "config.h"

#if HAVE_ALIGNOF
#define ALIGNOF(t) __alignof__(t)
#else
/* Alignment by measuring structure padding. */
#define ALIGNOF(t) (sizeof(struct { char c; t _h; }) - 1 - sizeof(t))
#endif

/* FIXME: Doesn't handle non-page-aligned poolsize. */

/* FIXME: Reduce. */
#define MIN_SIZE (getpagesize() * 2)

/* What's the granularity of sub-page allocs? */
#define BITMAP_GRANULARITY 4

/* File layout:
 *
 *  file := pagestates pad metadata
 *  pagestates := pages * 2-bits-per-page
 *  pad := pad to next ALIGNOF(metadata)
 *
 *  metadata := metalen next-ptr metabits
 *  metabits := freeblock | bitblock
 *  freeblock := 0+
 *  bitblock := 2-bits-per-bit-in-page 1
 */
struct metaheader
{
	/* Next meta header, or 0 */
	unsigned long next;
	/* Bits start here. */
};

/* Assumes a is a power of two. */
static unsigned long align_up(unsigned long x, unsigned long a)
{
	return (x + a - 1) & ~(a - 1);
}

static unsigned long div_up(unsigned long x, unsigned long a)
{
	return (x + a - 1) / a;
}

/* It turns out that we spend a lot of time dealing with bit pairs.
 * These routines manipulate them.
 */
static uint8_t get_bit_pair(const uint8_t *bits, unsigned long index)
{
	return bits[index * 2 / CHAR_BIT] >> (index * 2 % CHAR_BIT) & 3;
}

static void set_bit_pair(uint8_t *bits, unsigned long index, uint8_t val)
{
	bits[index * 2 / CHAR_BIT] &= ~(3 << (index * 2 % CHAR_BIT));
	bits[index * 2 / CHAR_BIT] |= (val << (index * 2 % CHAR_BIT));
}

/* This is used for page states and subpage allocations */
enum alloc_state
{
	FREE,
	TAKEN,
	TAKEN_START,
	SPECIAL,	/* Sub-page allocation for page states. */
};

/* The types for subpage metadata. */
enum sub_metadata_type
{
	/* FREE is same as alloc state */
	BITMAP = 1,
};

/* Page states are represented by bitpairs, at the start of the pool. */
#define BITS_PER_PAGE 2

static enum alloc_state get_page_state(const void *pool, unsigned long page)
{
	return get_bit_pair(pool, page);
}

static void set_page_state(void *pool, unsigned long page, enum alloc_state s)
{
	set_bit_pair(pool, page, s);
}

/* The offset of metadata for a subpage allocation is found at the end
 * of the subpage */
#define SUBPAGE_METAOFF (getpagesize() - sizeof(unsigned long))

/* This is the length of metadata in bits.  It consists of two bits
 * for every BITMAP_GRANULARITY of usable bytes in the page, then two
 * bits for the tailer.. */
#define BITMAP_METABITLEN						\
    ((div_up(SUBPAGE_METAOFF, BITMAP_GRANULARITY) + 1) * BITS_PER_PAGE)

/* This is the length in bytes. */
#define BITMAP_METALEN (div_up(BITMAP_METABITLEN, CHAR_BIT))

static struct metaheader *first_mheader(void *pool, unsigned long poolsize)
{
	unsigned int pagestatelen;

	pagestatelen = align_up(div_up(poolsize/getpagesize() * BITS_PER_PAGE,
				       CHAR_BIT),
				ALIGNOF(struct metaheader));
	return (struct metaheader *)((char *)pool + pagestatelen);
}

static struct metaheader *next_mheader(void *pool, struct metaheader *mh)
{
	if (!mh->next)
		return NULL;

	return (struct metaheader *)((char *)pool + mh->next);
}

static unsigned long pool_offset(void *pool, void *p)
{
	return (char *)p - (char *)pool;
}

void alloc_init(void *pool, unsigned long poolsize)
{
	/* FIXME: Alignment assumptions about pool. */
	unsigned long len, i;
	struct metaheader *mh;

	if (poolsize < MIN_SIZE)
		return;

	mh = first_mheader(pool, poolsize);

	/* Mark all page states FREE, and all of metaheader bitmap which takes
	 * rest of first page. */
	len = align_up(pool_offset(pool, mh + 1), getpagesize());
	BUILD_ASSERT(FREE == 0);
	memset(pool, 0, len);

	/* Mark the pagestate and metadata page(s) allocated. */
	set_page_state(pool, 0, TAKEN_START);
	for (i = 1; i < div_up(len, getpagesize()); i++)
		set_page_state(pool, i, TAKEN);
}

/* Two bits per element, representing page states.  Returns 0 on fail.
 * off is used to allocate from subpage bitmaps, which use the first 2
 * bits as the type, so the real bitmap is offset by 1. */
static unsigned long alloc_from_bitmap(uint8_t *bits, unsigned long off,
				       unsigned long elems,
				       unsigned long want, unsigned long align)
{
	long i;
	unsigned long free;

	free = 0;
	/* We allocate from far end, to increase ability to expand metadata. */
	for (i = elems - 1; i >= 0; i--) {
		switch (get_bit_pair(bits, off+i)) {
		case FREE:
			if (++free >= want) {
				unsigned long j;

				/* They might ask for large alignment. */
				if (align && i % align)
					continue;

				set_bit_pair(bits, off+i, TAKEN_START);
				for (j = i+1; j < i + want; j++)
					set_bit_pair(bits, off+j, TAKEN);
				return off+i;
			}
			break;
		case SPECIAL:
		case TAKEN_START:
		case TAKEN:
			free = 0;
			break;
		}
	}

	return 0;
}

static unsigned long alloc_get_pages(void *pool, unsigned long poolsize,
				     unsigned long pages, unsigned long align)
{
	return alloc_from_bitmap(pool, 0, poolsize / getpagesize(), pages,
				 align / getpagesize());
}

/* Offset to metadata is at end of page. */
static unsigned long *metadata_off(void *pool, unsigned long page)
{
	return (unsigned long *)
		((char *)pool + (page+1)*getpagesize() - sizeof(unsigned long));
}

static uint8_t *get_page_metadata(void *pool, unsigned long page)
{
	return (uint8_t *)pool + *metadata_off(pool, page);
}

static void set_page_metadata(void *pool, unsigned long page, uint8_t *meta)
{
	*metadata_off(pool, page) = meta - (uint8_t *)pool;
}

static unsigned long sub_page_alloc(void *pool, unsigned long page,
				    unsigned long size, unsigned long align)
{
	uint8_t *bits = get_page_metadata(pool, page);
	unsigned long i;

	/* TAKEN at start means a bitwise alloc. */
	assert(get_bit_pair(bits, 0) == BITMAP);

	/* We use a standart bitmap, but offset because of that BITMAP
	 * header. */
	i = alloc_from_bitmap(bits, 1, SUBPAGE_METAOFF/BITMAP_GRANULARITY,
			      div_up(size, BITMAP_GRANULARITY),
			      align / BITMAP_GRANULARITY);

	/* Can't allocate? */
	if (i == 0)
		return 0;

	/* i-1 because of the header. */
	return page*getpagesize() + (i-1)*BITMAP_GRANULARITY;
}

/* We look at the page states to figure out where the allocation for this
 * metadata ends. */
static unsigned long get_metalen(void *pool, unsigned long poolsize,
				 struct metaheader *mh)
{
	unsigned long i, first, pages = poolsize / getpagesize();

	first = pool_offset(pool, mh + 1)/getpagesize();

	for (i = first + 1; i < pages && get_page_state(pool,i) == TAKEN; i++);

	return i * getpagesize() - pool_offset(pool, mh + 1);
}

static uint8_t *alloc_metaspace(void *pool, unsigned long poolsize,
				struct metaheader *mh, unsigned long bytes,
				enum sub_metadata_type type)
{
	uint8_t *meta = (uint8_t *)(mh + 1);
	unsigned long free = 0, len, i, metalen;

	metalen = get_metalen(pool, poolsize, mh);

	/* TAKEN tags end a subpage alloc. */
	for (i = 0; i < metalen * CHAR_BIT / BITS_PER_PAGE; i += len) {
		switch (get_bit_pair(meta, i)) {
		case FREE:
			len = 1;
			free++;
			if (free == bytes * CHAR_BIT / BITS_PER_PAGE) {
				/* Mark this as a bitmap. */
				set_bit_pair(meta, i - free + 1, type);
				return meta + (i - free + 1)
					/ (CHAR_BIT / BITS_PER_PAGE);
			}
			break;
		case BITMAP:
			/* Skip over this allocated part. */
			len = BITMAP_METALEN * CHAR_BIT / BITS_PER_PAGE;
			free = 0;
			break;
		default:
			assert(0);
			return NULL;
		}
	}
	return NULL;
}

/* We need this many bytes of metadata. */
static uint8_t *new_metadata(void *pool, unsigned long poolsize,
			     unsigned long bytes, enum sub_metadata_type type)
{
	struct metaheader *mh, *newmh;
	unsigned long page;

	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		uint8_t *meta = alloc_metaspace(pool, poolsize, mh, bytes,type);

		if (meta)
			return meta;
	}

	/* No room for metadata?  Can we expand an existing one? */
	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		unsigned long nextpage;

		/* We start on this page. */
		nextpage = pool_offset(pool, (char *)(mh+1))/getpagesize();
		/* Iterate through any other pages we own. */
		while (get_page_state(pool, ++nextpage) == TAKEN)

		/* Now, can we grab that page? */
		if (get_page_state(pool, nextpage) != FREE)
			continue;

		/* OK, expand metadata, do it again. */
		set_page_state(pool, nextpage, TAKEN);
		BUILD_ASSERT(FREE == 0);
		memset((char *)pool + nextpage*getpagesize(), 0, getpagesize());
		return alloc_metaspace(pool, poolsize, mh, bytes, type);
	}

	/* No metadata left at all? */
	page = alloc_get_pages(pool, poolsize, div_up(bytes, getpagesize()), 1);
	if (!page)
		return NULL;

	newmh = (struct metaheader *)((char *)pool + page * getpagesize());
	BUILD_ASSERT(FREE == 0);
	memset(newmh + 1, 0, getpagesize() - sizeof(*mh));

	/* Sew it into linked list */
	mh = first_mheader(pool,poolsize);
	newmh->next = mh->next;
	mh->next = pool_offset(pool, newmh);

	return alloc_metaspace(pool, poolsize, newmh, bytes, type);
}

static void alloc_free_pages(void *pool, unsigned long pagenum)
{
	assert(get_page_state(pool, pagenum) == TAKEN_START);
	set_page_state(pool, pagenum, FREE);
	while (get_page_state(pool, ++pagenum) == TAKEN)
		set_page_state(pool, pagenum, FREE);
}

static unsigned long alloc_sub_page(void *pool, unsigned long poolsize,
				    unsigned long size, unsigned long align)
{
	unsigned long i;
	uint8_t *metadata;

	/* Look for partial page. */
	for (i = 0; i < poolsize / getpagesize(); i++) {
		unsigned long ret;
		if (get_page_state(pool, i) != SPECIAL)
			continue;

		ret = sub_page_alloc(pool, i, size, align);
		if (ret)
			return ret;
	}

	/* Create new SUBPAGE page. */
	i = alloc_get_pages(pool, poolsize, 1, 1);
	if (i == 0)
		return 0;

	/* Get metadata for page. */
	metadata = new_metadata(pool, poolsize, BITMAP_METALEN, BITMAP);
	if (!metadata) {
		alloc_free_pages(pool, i);
		return 0;
	}

	/* Actually, this is a subpage page now. */
	set_page_state(pool, i, SPECIAL);

	/* Set metadata pointer for page. */
	set_page_metadata(pool, i, metadata);

	/* Do allocation like normal */
	return sub_page_alloc(pool, i, size, align);
}

/* Returns true if we cleaned any pages. */
static bool clean_empty_subpages(void *pool, unsigned long poolsize)
{
	unsigned long i;
	bool progress = false;

	for (i = 0; i < poolsize/getpagesize(); i++) {
		uint8_t *meta;
		unsigned int j;
		if (get_page_state(pool, i) != SPECIAL)
			continue;

		meta = get_page_metadata(pool, i);
		/* Skip the header (first bit of metadata). */
		for (j = 1; j < SUBPAGE_METAOFF/BITMAP_GRANULARITY+1; j++)
			if (get_bit_pair(meta, j) != FREE)
				break;

		/* So, is this page totally empty? */
		if (j == SUBPAGE_METAOFF/BITMAP_GRANULARITY+1) {
			set_page_state(pool, i, FREE);
			progress = true;
		}
	}
	return progress;
}

/* Returns true if we cleaned any pages. */
static bool clean_metadata(void *pool, unsigned long poolsize)
{
	struct metaheader *mh, *prev_mh = NULL;
	bool progress = false;

	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		uint8_t *meta;
		long i;
		unsigned long metalen = get_metalen(pool, poolsize, mh);

		meta = (uint8_t *)(mh + 1);
		BUILD_ASSERT(FREE == 0);
		for (i = metalen - 1; i > 0; i--)
			if (meta[i] != 0)
				break;

		/* Completely empty? */
		if (prev_mh && i == metalen) {
			alloc_free_pages(pool,
					 pool_offset(pool, mh)/getpagesize());
			prev_mh->next = mh->next;
			mh = prev_mh;
			progress = true;
		} else {
			uint8_t *p;

			/* Some pages at end are free? */
			for (p = (uint8_t *)(mh+1) + metalen - getpagesize();
			     p > meta + i;
			     p -= getpagesize()) {
				set_page_state(pool,
					       pool_offset(pool, p)
					       / getpagesize(),
					       FREE);
				progress = true;
			}
		}
	}

	return progress;
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	bool subpage_clean = false, metadata_clean = false;
	unsigned long ret;

	if (poolsize < MIN_SIZE)
		return NULL;

again:
	/* Sub-page allocations have an overhead of ~12%. */
	if (size + size/8 >= getpagesize() || align >= getpagesize()) {
		unsigned long pages = div_up(size, getpagesize());

		ret = alloc_get_pages(pool, poolsize, pages, align)
			* getpagesize();
	} else
		ret = alloc_sub_page(pool, poolsize, size, align);

	if (ret != 0)
		return (char *)pool + ret;

	/* Allocation failed: garbage collection. */
	if (!subpage_clean) {
		subpage_clean = true;
		if (clean_empty_subpages(pool, poolsize))
			goto again;
	}

	if (!metadata_clean) {
		metadata_clean = true;
		if (clean_metadata(pool, poolsize))
			goto again;
	}

	/* FIXME: Compact metadata? */
	return NULL;
}

static void subpage_free(void *pool, unsigned long pagenum, void *free)
{
	unsigned long off = (unsigned long)free % getpagesize();
	uint8_t *metadata;

	assert(off < SUBPAGE_METAOFF);
	assert(off % BITMAP_GRANULARITY == 0);

	metadata = get_page_metadata(pool, pagenum);

	off /= BITMAP_GRANULARITY;

	/* Offset by one because first bit is used for header. */
	off++;

	set_bit_pair(metadata, off++, FREE);
	while (off < SUBPAGE_METAOFF / BITMAP_GRANULARITY
	       && get_bit_pair(metadata, off) == TAKEN)
		set_bit_pair(metadata, off++, FREE);
}

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	unsigned long pagenum;
	struct metaheader *mh;

	if (!free)
		return;

	assert(poolsize >= MIN_SIZE);

	mh = first_mheader(pool, poolsize);
	assert((char *)free >= (char *)(mh + 1));
	assert((char *)pool + poolsize > (char *)free);

	pagenum = pool_offset(pool, free) / getpagesize();

	if (get_page_state(pool, pagenum) == SPECIAL)
		subpage_free(pool, pagenum, free);
	else {
		assert((unsigned long)free % getpagesize() == 0);
		alloc_free_pages(pool, pagenum);
	}
}

static bool is_metadata_page(void *pool, unsigned long poolsize,
			     unsigned long page)
{
	struct metaheader *mh;

	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		unsigned long start, end;

		start = pool_offset(pool, mh);
		end = pool_offset(pool, (char *)(mh+1)
				  + get_metalen(pool, poolsize, mh));
		if (page >= start/getpagesize() && page < end/getpagesize())
			return true;
	}
	return false;
}

static bool check_subpage(void *pool, unsigned long poolsize,
			  unsigned long page)
{
	unsigned long *mhoff = metadata_off(pool, page);
	unsigned int i;
	enum alloc_state last_state = FREE;

	if (*mhoff + sizeof(struct metaheader) > poolsize)
		return false;

	if (*mhoff % ALIGNOF(struct metaheader) != 0)
		return false;

	/* It must point to a metadata page. */
	if (!is_metadata_page(pool, poolsize, *mhoff / getpagesize()))
		return false;

	/* Header at start of subpage allocation */
	if (get_bit_pair((uint8_t *)pool + *mhoff, 0) != BITMAP)
		return false;

	for (i = 0; i < SUBPAGE_METAOFF / BITMAP_GRANULARITY; i++) {
		enum alloc_state state;

		/* +1 because header is the first byte. */
		state = get_bit_pair((uint8_t *)pool + *mhoff, i+1);
		switch (state) {
		case SPECIAL:
			return false;
		case TAKEN:
			if (last_state == FREE)
				return false;
			break;
		default:
			break;
		}
		last_state = state;
	}
	return true;
}

bool alloc_check(void *pool, unsigned long poolsize)
{
	unsigned long i;
	struct metaheader *mh;
	enum alloc_state last_state = FREE;
	bool was_metadata = false;

	if (poolsize < MIN_SIZE)
		return true;

	if (get_page_state(pool, 0) != TAKEN_START)
		return false;

	/* First check metadata pages. */
	/* Metadata pages will be marked TAKEN. */
	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		unsigned long start, end;

		start = pool_offset(pool, mh);
		if (start + sizeof(*mh) > poolsize)
			return false;

		end = pool_offset(pool, (char *)(mh+1)
				  + get_metalen(pool, poolsize, mh));
		if (end > poolsize)
			return false;

		/* Non-first pages should start on a page boundary. */
		if (mh != first_mheader(pool, poolsize)
		    && start % getpagesize() != 0)
			return false;

		/* It should end on a page boundary. */
		if (end % getpagesize() != 0)
			return false;
	}

	for (i = 0; i < poolsize / getpagesize(); i++) {
		enum alloc_state state = get_page_state(pool, i);
		bool is_metadata = is_metadata_page(pool, poolsize,i);

		switch (state) {
		case FREE:
			/* metadata pages are never free. */
			if (is_metadata)
				return false;
		case TAKEN_START:
			break;
		case TAKEN:
			/* This should continue a previous block. */
			if (last_state == FREE)
				return false;
			if (is_metadata != was_metadata)
				return false;
			break;
		case SPECIAL:
			/* Check metadata pointer etc. */
			if (!check_subpage(pool, poolsize, i))
				return false;
		}
		last_state = state;
		was_metadata = is_metadata;
	}
	return true;
}

void alloc_visualize(FILE *out, void *pool, unsigned long poolsize)
{
	struct metaheader *mh;
	unsigned long pagebitlen, metadata_pages, count[1<<BITS_PER_PAGE], tot;
	long i;

	if (poolsize < MIN_SIZE) {
		fprintf(out, "Pool smaller than %u: no content\n", MIN_SIZE);
		return;
	}

	memset(count, 0, sizeof(count));
	for (i = 0; i < poolsize / getpagesize(); i++)
		count[get_page_state(pool, i)]++;

	mh = first_mheader(pool, poolsize);
	pagebitlen = (char *)mh - (char *)pool;
	fprintf(out, "%lu bytes of page bits: FREE/TAKEN/TAKEN_START/SUBPAGE = %lu/%lu/%lu/%lu\n",
		pagebitlen, count[0], count[1], count[2], count[3]);

	/* One metadata page for every page of page bits. */
	metadata_pages = div_up(pagebitlen, getpagesize());

	/* Now do each metadata page. */
	for (; mh; mh = next_mheader(pool,mh)) {
		unsigned long free = 0, subpageblocks = 0, len = 0, metalen;
		uint8_t *meta = (uint8_t *)(mh + 1);

		metalen = get_metalen(pool, poolsize, mh);
		metadata_pages += (sizeof(*mh) + metalen) / getpagesize();

		for (i = 0; i < metalen * CHAR_BIT / BITS_PER_PAGE; i += len) {
			switch (get_page_state(meta, i)) {
			case FREE:
				len = 1;
				free++;
				break;
			case BITMAP:
				/* Skip over this allocated part. */
				len = BITMAP_METALEN * CHAR_BIT;
				subpageblocks++;
				break;
			default:
				assert(0);
			}
		}

		fprintf(out, "Metadata %lu-%lu: %lu free, %lu subpageblocks, %lu%% density\n",
			pool_offset(pool, mh),
			pool_offset(pool, (char *)(mh+1) + metalen),
			free, subpageblocks,
			subpageblocks * BITMAP_METALEN * 100
			/ (free + subpageblocks * BITMAP_METALEN));
	}

	/* Account for total pages allocated. */
	tot = (count[1] + count[2] - metadata_pages) * getpagesize();

	fprintf(out, "Total metadata bytes = %lu\n",
		metadata_pages * getpagesize());

	/* Now do every subpage. */
	for (i = 0; i < poolsize / getpagesize(); i++) {
		uint8_t *meta;
		unsigned int j;
		if (get_page_state(pool, i) != SPECIAL)
			continue;

		memset(count, 0, sizeof(count));
		meta = get_page_metadata(pool, i);
		for (j = 0; j < SUBPAGE_METAOFF/BITMAP_GRANULARITY; j++)
			count[get_page_state(meta, j)]++;

		fprintf(out, "Subpage %lu: "
			"FREE/TAKEN/TAKEN_START = %lu/%lu/%lu %lu%% density\n",
			i, count[0], count[1], count[2],
			((count[1] + count[2]) * BITMAP_GRANULARITY) * 100
			/ getpagesize());
		tot += (count[1] + count[2]) * BITMAP_GRANULARITY;
	}

	/* This is optimistic, since we overalloc in several cases. */
	fprintf(out, "Best possible allocation density = %lu%%\n",
		tot * 100 / poolsize);
}
