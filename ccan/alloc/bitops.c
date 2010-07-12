#include "bitops.h"
#include "config.h"
#include <ccan/short_types/short_types.h>
#include <limits.h>

unsigned int fls(unsigned long val)
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
unsigned int ffsl(unsigned long val)
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

unsigned int popcount(unsigned long val)
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

unsigned long align_up(unsigned long x, unsigned long align)
{
	return (x + align - 1) & ~(align - 1);
}
