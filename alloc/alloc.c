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
	/* Length (after this header).  (FIXME: Could be in pages). */
	unsigned long metalen;
	/* Next meta header, or 0 */
	unsigned long next;
	/* Bits start here. */
};

#define BITS_PER_PAGE 2
/* FIXME: Don't use page states for bitblock.  It's tacky and confusing. */
enum page_state
{
	FREE,
	TAKEN,
	TAKEN_START,
	SUBPAGE,
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

/* The offset of metadata for a subpage allocation is found at the end
 * of the subpage */
#define SUBPAGE_METAOFF (getpagesize() - sizeof(unsigned long))

/* This is the length of metadata in bits.  It consists of two bits
 * for every BITMAP_GRANULARITY of usable bytes in the page, then two
 * bits for the TAKEN tailer.. */
#define BITMAP_METABITLEN						\
    ((div_up(SUBPAGE_METAOFF, BITMAP_GRANULARITY) + 1) * BITS_PER_PAGE)

/* This is the length in bytes. */
#define BITMAP_METALEN (div_up(BITMAP_METABITLEN, CHAR_BIT))

static enum page_state get_page_state(const uint8_t *bits, unsigned long page)
{
	return bits[page * 2 / CHAR_BIT] >> (page * 2 % CHAR_BIT) & 3;
}

static void set_page_state(uint8_t *bits, unsigned long page, enum page_state s)
{
	bits[page * 2 / CHAR_BIT] &= ~(3 << (page * 2 % CHAR_BIT));
	bits[page * 2 / CHAR_BIT] |= ((uint8_t)s << (page * 2 % CHAR_BIT));
}

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

	/* len covers all page states, plus the metaheader. */
	len = (char *)(mh + 1) - (char *)pool;
	/* Mark all page states FREE */
	BUILD_ASSERT(FREE == 0);
	memset(pool, 0, len);

	/* metaheader len takes us up to next page boundary. */
	mh->metalen = align_up(len, getpagesize()) - len;

	/* Mark the pagestate and metadata page(s) allocated. */
	set_page_state(pool, 0, TAKEN_START);
	for (i = 1; i < div_up(len, getpagesize()); i++)
		set_page_state(pool, i, TAKEN);
}

/* Two bits per element, representing page states.  Returns 0 on fail. */
static unsigned long alloc_from_bitmap(uint8_t *bits, unsigned long elems,
				       unsigned long want, unsigned long align)
{
	long i;
	unsigned long free;

	free = 0;
	/* We allocate from far end, to increase ability to expand metadata. */
	for (i = elems - 1; i >= 0; i--) {
		switch (get_page_state(bits, i)) {
		case FREE:
			if (++free >= want) {
				unsigned long j;

				/* They might ask for large alignment. */
				if (align && i % align)
					continue;

				for (j = i+1; j < i + want; j++)
					set_page_state(bits, j, TAKEN);
				set_page_state(bits, i, TAKEN_START);
				return i;
			}
			break;
		case SUBPAGE:
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
	long i;
	unsigned long free;

	free = 0;
	/* We allocate from far end, to increase ability to expand metadata. */
	for (i = poolsize / getpagesize() - 1; i >= 0; i--) {
		switch (get_page_state(pool, i)) {
		case FREE:
			if (++free >= pages) {
				unsigned long j, addr;

				addr = (unsigned long)pool + i * getpagesize();

				/* They might ask for multi-page alignment. */
				if (addr % align)
					continue;

				for (j = i+1; j < i + pages; j++)
					set_page_state(pool, j, TAKEN);
				set_page_state(pool, i, TAKEN_START);
				return i;
			}
			break;
		case SUBPAGE:
		case TAKEN_START:
		case TAKEN:
			free = 0;
			break;
		}
	}

	return 0;
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

static void *sub_page_alloc(void *pool, unsigned long page,
			    unsigned long size, unsigned long align)
{
	uint8_t *bits = get_page_metadata(pool, page);
	unsigned long i;

	/* TAKEN at end means a bitwise alloc. */
	assert(get_page_state(bits, getpagesize()/BITMAP_GRANULARITY - 1)
	       == TAKEN);

	/* Our bits are the same as the page bits. */
	i = alloc_from_bitmap(bits, SUBPAGE_METAOFF/BITMAP_GRANULARITY,
			      div_up(size, BITMAP_GRANULARITY),
			      align / BITMAP_GRANULARITY);

	/* Can't allocate? */
	if (i == 0)
		return NULL;

	return (char *)pool + page*getpagesize() + i*BITMAP_GRANULARITY;
}

static uint8_t *alloc_metaspace(struct metaheader *mh, unsigned long bytes)
{
	uint8_t *meta = (uint8_t *)(mh + 1);
	unsigned long free = 0, len;
	long i;

	/* TAKEN tags end a subpage alloc. */
	for (i = mh->metalen * CHAR_BIT / BITS_PER_PAGE - 1; i >= 0; i -= len) {
		switch (get_page_state(meta, i)) {
		case FREE:
			len = 1;
			free++;
			if (free == bytes * CHAR_BIT / BITS_PER_PAGE) {
				/* TAKEN marks end of metablock. */
				set_page_state(meta, i + free - 1, TAKEN);
				return meta + i / (CHAR_BIT / BITS_PER_PAGE);
			}
			break;
		case TAKEN:
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
			     unsigned long bytes)
{
	struct metaheader *mh, *newmh;
	unsigned long page;

	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		uint8_t *meta = alloc_metaspace(mh, bytes);

		if (meta)
			return meta;
	}

	/* No room for metadata?  Can we expand an existing one? */
	for (mh = first_mheader(pool,poolsize); mh; mh = next_mheader(pool,mh)){
		/* It should end on a page boundary. */
		unsigned long nextpage;

		nextpage = pool_offset(pool, (char *)(mh + 1) + mh->metalen);
		assert(nextpage % getpagesize() == 0);

		/* Now, can we grab that page? */
		if (get_page_state(pool, nextpage / getpagesize()) != FREE)
			continue;

		/* OK, expand metadata, do it again. */
		set_page_state(pool, nextpage / getpagesize(), TAKEN);
		BUILD_ASSERT(FREE == 0);
		memset((char *)pool + nextpage, 0, getpagesize());
		mh->metalen += getpagesize();
		return alloc_metaspace(mh, bytes);
	}

	/* No metadata left at all? */
	page = alloc_get_pages(pool, poolsize, div_up(bytes, getpagesize()), 1);
	if (!page)
		return NULL;

	newmh = (struct metaheader *)((char *)pool + page * getpagesize());
	newmh->metalen = getpagesize() - sizeof(*mh);
	BUILD_ASSERT(FREE == 0);
	memset(newmh + 1, 0, newmh->metalen);

	/* Sew it into linked list */
	mh = first_mheader(pool,poolsize);
	newmh->next = mh->next;
	mh->next = (char *)newmh - (char *)pool;

	return alloc_metaspace(newmh, bytes);
}

static void alloc_free_pages(void *pool, unsigned long pagenum)
{
	assert(get_page_state(pool, pagenum) == TAKEN_START);
	set_page_state(pool, pagenum, FREE);
	while (get_page_state(pool, ++pagenum) == TAKEN)
		set_page_state(pool, pagenum, FREE);
}

static void *alloc_sub_page(void *pool, unsigned long poolsize,
			    unsigned long size, unsigned long align)
{
	unsigned long i;
	uint8_t *metadata;

	/* Look for partial page. */
	for (i = 0; i < poolsize / getpagesize(); i++) {
		void *ret;
		if (get_page_state(pool, i) != SUBPAGE)
			continue;

		ret = sub_page_alloc(pool, i, size, align);
		if (ret)
			return ret;
	}

	/* Create new SUBPAGE page. */
	i = alloc_get_pages(pool, poolsize, 1, 1);
	if (i == 0)
		return NULL;

	/* Get metadata for page. */
	metadata = new_metadata(pool, poolsize, BITMAP_METALEN);
	if (!metadata) {
		alloc_free_pages(pool, i);
		return NULL;
	}

	/* Actually, this is a SUBPAGE page now. */
	set_page_state(pool, i, SUBPAGE);

	/* Set metadata pointer for page. */
	set_page_metadata(pool, i, metadata);

	/* Do allocation like normal */
	return sub_page_alloc(pool, i, size, align);
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	if (poolsize < MIN_SIZE)
		return NULL;

	/* Sub-page allocations have an overhead of 25%. */
	if (size + size/4 >= getpagesize() || align >= getpagesize()) {
		unsigned long ret, pages = div_up(size, getpagesize());

		ret = alloc_get_pages(pool, poolsize, pages, align);
		if (ret == 0)
			return NULL;
		return (char *)pool + ret * getpagesize();
	}

	return alloc_sub_page(pool, poolsize, size, align);
}

static void subpage_free(void *pool, unsigned long pagenum, void *free)
{
	unsigned long off = (unsigned long)free % getpagesize();
	uint8_t *metadata;

	assert(off < SUBPAGE_METAOFF);
	assert(off % BITMAP_GRANULARITY == 0);

	metadata = get_page_metadata(pool, pagenum);

	off /= BITMAP_GRANULARITY;

	set_page_state(metadata, off++, FREE);
	while (off < SUBPAGE_METAOFF / BITMAP_GRANULARITY
	       && get_page_state(metadata, off) == TAKEN)
		set_page_state(metadata, off++, FREE);

	/* FIXME: If whole page free, free page and metadata. */
}

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	unsigned long pagenum;
	struct metaheader *mh;

	if (!free)
		return;

	assert(poolsize >= MIN_SIZE);

	mh = first_mheader(pool, poolsize);
	assert((char *)free >= (char *)(mh + 1) + mh->metalen);
	assert((char *)pool + poolsize > (char *)free);

	pagenum = pool_offset(pool, free) / getpagesize();

	if (get_page_state(pool, pagenum) == SUBPAGE)
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
		end = pool_offset(pool, (char *)(mh+1) + mh->metalen);
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
	enum page_state last_state = FREE;

	if (*mhoff + sizeof(struct metaheader) > poolsize)
		return false;

	if (*mhoff % ALIGNOF(struct metaheader) != 0)
		return false;

	/* It must point to a metadata page. */
	if (!is_metadata_page(pool, poolsize, *mhoff / getpagesize()))
		return false;

	/* Marker at end of subpage allocation is "taken" */
	if (get_page_state((uint8_t *)pool + *mhoff,
			   getpagesize()/BITMAP_GRANULARITY - 1) != TAKEN)
		return false;

	for (i = 0; i < SUBPAGE_METAOFF / BITMAP_GRANULARITY; i++) {
		enum page_state state;

		state = get_page_state((uint8_t *)pool + *mhoff, i);
		switch (state) {
		case SUBPAGE:
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
	enum page_state last_state = FREE;
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

		end = pool_offset(pool, (char *)(mh+1) + mh->metalen);
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
		enum page_state state = get_page_state(pool, i);
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
		case SUBPAGE:
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
		unsigned long free = 0, subpageblocks = 0, len = 0;
		uint8_t *meta = (uint8_t *)(mh + 1);

		metadata_pages += (sizeof(*mh) + mh->metalen) / getpagesize();

		/* TAKEN tags end a subpage alloc. */
		for (i = mh->metalen * CHAR_BIT/BITS_PER_PAGE - 1;
		     i >= 0;
		     i -= len) {
			switch (get_page_state(meta, i)) {
			case FREE:
				len = 1;
				free++;
				break;
			case TAKEN:
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
			pool_offset(pool, (char *)(mh+1) + mh->metalen),
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
		if (get_page_state(pool, i) != SUBPAGE)
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
