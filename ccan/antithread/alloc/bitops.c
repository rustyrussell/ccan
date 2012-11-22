/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include "bitops.h"
#include "config.h"
#include <ccan/build_assert/build_assert.h>
#include <ccan/short_types/short_types.h>
#include <ccan/ilog/ilog.h>
#include <limits.h>

unsigned int afls(unsigned long val)
{
	BUILD_ASSERT(sizeof(val) == sizeof(u32) || sizeof(val) == sizeof(u64));
	if (sizeof(val) == sizeof(u32))
		return ilog32(val);
	else
		return ilog64(val);
}

/* FIXME: Move to bitops. */
unsigned int affsl(unsigned long val)
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
			+ ((v >> 2) & 0x3333333333333333ULL);
		v = (v & 0x0F0F0F0F0F0F0F0FULL)
			+ ((v >> 4) & 0x0F0F0F0F0F0F0F0FULL);
		v = (v & 0x00FF00FF00FF00FFULL)
			+ ((v >> 8) & 0x00FF00FF00FF00FFULL);
		v = (v & 0x0000FFFF0000FFFFULL)
			+ ((v >> 16) & 0x0000FFFF0000FFFFULL);
		v = (v & 0x00000000FFFFFFFFULL)
			+ ((v >> 32) & 0x00000000FFFFFFFFULL);
		return v;
	}
	val = (val & 0x55555555ULL) + ((val >> 1) & 0x55555555ULL);
	val = (val & 0x33333333ULL) + ((val >> 2) & 0x33333333ULL);
	val = (val & 0x0F0F0F0FULL) + ((val >> 4) & 0x0F0F0F0FULL);
	val = (val & 0x00FF00FFULL) + ((val >> 8) & 0x00FF00FFULL);
	val = (val & 0x0000FFFFULL) + ((val >> 16) & 0x0000FFFFULL);
	return val;
#endif
}

unsigned long align_up(unsigned long x, unsigned long align)
{
	return (x + align - 1) & ~(align - 1);
}
