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

/* Metadata looks like this:
 * <unsigned long metalen> <page states> <align-pad> [<1-byte-len> <free|bit|uniform> bits...]* [unsigned long next]. 
 */

#define BITS_PER_PAGE 2
enum page_state
{
	FREE,
	TAKEN,
	TAKEN_START,
};

static enum page_state get_page_state(const uint8_t *bits, unsigned long page)
{
	bits += sizeof(unsigned long);
	return bits[page * 2 / CHAR_BIT] >> (page * 2 % CHAR_BIT) & 3;
}

static void set_page_state(uint8_t *bits, unsigned long page, enum page_state s)
{
	bits += sizeof(unsigned long);
	bits[page * 2 / CHAR_BIT] &= ~(3 << (page * 2 % CHAR_BIT));
	bits[page * 2 / CHAR_BIT] |= ((uint8_t)s << (page * 2 % CHAR_BIT));
}

/* Assumes a is a power of two. */
static unsigned long align_up(unsigned long x, unsigned long a)
{
	return (x + a - 1) & ~(a - 1);
}

static unsigned long div_up(unsigned long x, unsigned long a)
{
	return (x + a - 1) / a;
}

static unsigned long metadata_length(void *pool, unsigned long poolsize)
{
	return *(unsigned long *)pool;
}

void alloc_init(void *pool, unsigned long poolsize)
{
	/* FIXME: Alignment assumptions about pool. */
	unsigned long *metalen = pool, pages, pagestatebytes, i;

	if (poolsize < MIN_SIZE)
		return;

	pages = poolsize / getpagesize();

	/* First comes the metadata length, then 2 bits per page, then
	 * the next pointer. */
	pagestatebytes = div_up(pages * BITS_PER_PAGE, CHAR_BIT);
	*metalen = sizeof(*metalen)
		+ align_up(pagestatebytes, ALIGNOF(unsigned long))
		+ sizeof(unsigned long);

	/* Mark all the bits FREE to start, and zero the next pointer. */
	BUILD_ASSERT(FREE == 0);
	memset(metalen + 1, 0, *metalen - sizeof(*metalen));

	/* Mark the metadata page(s) allocated. */
	set_page_state(pool, 0, TAKEN_START);
	for (i = 1; i < div_up(*metalen, getpagesize()); i++)
		set_page_state(pool, i, TAKEN);
}

static void *alloc_get_pages(void *pool, unsigned long poolsize,
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
				unsigned long j;
				char *ret = (char *)pool + i * getpagesize();

				/* They might ask for multi-page alignment. */
				if ((unsigned long)ret % align)
					continue;

				for (j = i+1; j < i + pages; j++)
					set_page_state(pool, j, TAKEN);
				set_page_state(pool, i, TAKEN_START);
				return ret;
			}
			break;
		case TAKEN_START:
		case TAKEN:
			free = 0;
			break;
		}
	}

	return NULL;
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	if (poolsize < MIN_SIZE)
		return NULL;

	if (size >= getpagesize() || align > getpagesize()) {
		unsigned long pages = (size + getpagesize()-1) / getpagesize();
		return alloc_get_pages(pool, poolsize, pages, align);
	}

	/* FIXME: Sub-page allocations. */
	return alloc_get_pages(pool, poolsize, 1, align);
}

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	unsigned long pagenum, metalen;

	if (!free)
		return;

	assert(poolsize >= MIN_SIZE);

	metalen = metadata_length(pool, poolsize);

	assert((char *)free >= (char *)pool + metalen);
	assert((char *)pool + poolsize > (char *)free);
	assert((unsigned long)free % getpagesize() == 0);

	pagenum = ((char *)free - (char *)pool) / getpagesize();

	assert(get_page_state(pool, pagenum) == TAKEN_START);
	set_page_state(pool, pagenum, FREE);
	while (get_page_state(pool, ++pagenum) == TAKEN)
		set_page_state(pool, pagenum, FREE);
}

bool alloc_check(void *pool, unsigned long poolsize)
{
	unsigned long i, metalen, pagestatebytes;
	enum page_state last_state = FREE;

	if (poolsize < MIN_SIZE)
		return true;

	metalen = metadata_length(pool, poolsize);
	if (get_page_state(pool, 0) != TAKEN_START)
		return false;

	pagestatebytes = div_up(poolsize / getpagesize() * BITS_PER_PAGE,
				CHAR_BIT);
	if (metalen < (sizeof(unsigned long)
		       + align_up(pagestatebytes, ALIGNOF(unsigned long))
		       + sizeof(unsigned long)))
		return false;

	for (i = 1; i < poolsize / getpagesize(); i++) {
		enum page_state state = get_page_state(pool, i);

		/* Metadata pages will be marked TAKEN. */
		if (i < div_up(metalen, getpagesize())) {
			if (get_page_state(pool, 0) != TAKEN)
				return false;
			continue;
		}

		switch (state) {
		case FREE:
		case TAKEN_START:
			break;
		case TAKEN:
			if (last_state == FREE)
				return false;
			break;
		default:
			return false;
		}
		last_state = state;
	}
	return true;
}
