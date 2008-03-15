#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "alloc.h"

/* FIXME: Doesn't handle non-page-aligned poolsize. */
/* FIXME: Doesn't handle sub-page allocations. */

#define MIN_SIZE (getpagesize() * 2)

enum page_state
{
	FREE,
	TAKEN,
	TAKEN_START,
};

void alloc_init(void *pool, unsigned long poolsize)
{
	uint8_t *bits = pool;
	unsigned int pages = poolsize / getpagesize();

	if (poolsize < MIN_SIZE)
		return;

	memset(bits, 0, (pages * 2 + CHAR_BIT - 1)/ CHAR_BIT);
}

static enum page_state get_page_state(const uint8_t *bits, unsigned long page)
{
	return bits[page * 2 / CHAR_BIT] >> (page * 2 % CHAR_BIT) & 3;
}

static void set_page_state(uint8_t *bits, unsigned long page, enum page_state s)
{
	bits[page * 2 / CHAR_BIT] &= ~(3 << (page * 2 % CHAR_BIT));
	bits[page * 2 / CHAR_BIT] |= ((uint8_t)s << (page * 2 % CHAR_BIT));
}

static unsigned long metadata_length(void *pool, unsigned long poolsize)
{
	return ((poolsize / getpagesize() * 2 / CHAR_BIT) + getpagesize() - 1)
		& ~(getpagesize() - 1);
}

void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align)
{
	unsigned long i, free, want, metalen;

	if (poolsize < MIN_SIZE)
		return NULL;

	/* FIXME: Necessary for this. */
	if (size == 0)
		size = 1;

	want = (size + getpagesize() - 1) / getpagesize();
	metalen = metadata_length(pool, poolsize);

	free = 0;
	for (i = 0; i < (poolsize - metalen) / getpagesize(); i++) {
		switch (get_page_state(pool, i)) {
		case FREE:
			if (++free >= want) {
				unsigned int j;
				char *ret = (char *)pool + metalen
					+ (i - want + 1) * getpagesize();

				if ((unsigned long)ret % align)
					continue;

				for (j = i; j > i - want + 1; j--)
					set_page_state(pool, j, TAKEN);
				set_page_state(pool, i - want + 1, TAKEN_START);
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

void alloc_free(void *pool, unsigned long poolsize, void *free)
{
	unsigned long pagenum, metalen;

	if (poolsize < MIN_SIZE)
		return;

	if (!free)
		return;

	metalen = metadata_length(pool, poolsize);

	assert(free > pool && (char *)pool + poolsize > (char *)free);
	assert((unsigned long)free % getpagesize() == 0);

	pagenum = ((char *)free - (char *)pool - metalen) / getpagesize();

	assert(get_page_state(pool, pagenum) == TAKEN_START);
	set_page_state(pool, pagenum, FREE);
	while (get_page_state(pool, ++pagenum) == TAKEN)
		set_page_state(pool, pagenum, FREE);
}

bool alloc_check(void *pool, unsigned long poolsize)
{
	unsigned int i, metalen;
	enum page_state last_state = FREE;

	if (poolsize < MIN_SIZE)
		return true;

	metalen = metadata_length(pool, poolsize);
	for (i = 0; i < (poolsize - metalen) / getpagesize(); i++) {
		enum page_state state = get_page_state(pool, i);
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
