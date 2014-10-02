/* Licensed under LGPLv2.1+ - see LICENSE file for details */

#include "config.h"

#include <ccan/bitmap/bitmap.h>

#include <assert.h>

#define BIT_ALIGN_DOWN(n)	((n) & ~(BITMAP_WORD_BITS - 1))
#define BIT_ALIGN_UP(n)		BIT_ALIGN_DOWN((n) + BITMAP_WORD_BITS - 1)

void bitmap_zero_range(bitmap *bitmap, unsigned long n, unsigned long m)
{
	unsigned long an = BIT_ALIGN_UP(n);
	unsigned long am = BIT_ALIGN_DOWN(m);
	bitmap_word headmask = -1ULL >> (n % BITMAP_WORD_BITS);
	bitmap_word tailmask = ~(-1ULL >> (m % BITMAP_WORD_BITS));

	assert(m >= n);

	if (am < an) {
		BITMAP_WORD(bitmap, n) &= ~bitmap_bswap(headmask & tailmask);
		return;
	}

	if (an > n)
		BITMAP_WORD(bitmap, n) &= ~bitmap_bswap(headmask);

	if (am > an)
		memset(&BITMAP_WORD(bitmap, an), 0,
		       (am - an) / BITMAP_WORD_BITS * sizeof(bitmap_word));

	if (m > am)
		BITMAP_WORD(bitmap, m) &= ~bitmap_bswap(tailmask);
}

void bitmap_fill_range(bitmap *bitmap, unsigned long n, unsigned long m)
{
	unsigned long an = BIT_ALIGN_UP(n);
	unsigned long am = BIT_ALIGN_DOWN(m);
	bitmap_word headmask = -1ULL >> (n % BITMAP_WORD_BITS);
	bitmap_word tailmask = ~(-1ULL >> (m % BITMAP_WORD_BITS));

	assert(m >= n);

	if (am < an) {
		BITMAP_WORD(bitmap, n) |= bitmap_bswap(headmask & tailmask);
		return;
	}

	if (an > n)
		BITMAP_WORD(bitmap, n) |= bitmap_bswap(headmask);

	if (am > an)
		memset(&BITMAP_WORD(bitmap, an), 0xff,
		       (am - an) / BITMAP_WORD_BITS * sizeof(bitmap_word));

	if (m > am)
		BITMAP_WORD(bitmap, m) |= bitmap_bswap(tailmask);
}
