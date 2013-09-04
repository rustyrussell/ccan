/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_BITMAP_H_
#define CCAN_BITMAP_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define BITS_PER_LONG	(sizeof(unsigned long) * 8)

/*
 * We use an unsigned long to get alignment, but wrap it in a
 * structure for type checking
 */
typedef struct {
	unsigned long l;
} bitmap;

#define BYTE(_bm, _n)	(((unsigned char *)(_bm))[(_n) / 8])
#define LONG(_bm, _n)	(((unsigned long *)(_bm))[(_n) / BITS_PER_LONG])
#define BIT(_n)		(0x80 >> ((_n) % 8))

#define BYTES(_nbits)	((_nbits) / 8)
#define BITS(_nbits)	((~(0xff >> ((_nbits) % 8))) & 0xff)

static inline void bitmap_set_bit(bitmap *bitmap, int n)
{
	BYTE(bitmap, n) |= BIT(n);
}

static inline void bitmap_clear_bit(bitmap *bitmap, int n)
{
	BYTE(bitmap, n) &= ~BIT(n);
}

static inline void bitmap_change_bit(bitmap *bitmap, int n)
{
	BYTE(bitmap, n) ^= BIT(n);
}

static inline bool bitmap_test_bit(bitmap *bitmap, int n)
{
	return !!(BYTE(bitmap, n) & BIT(n));
}


static inline void bitmap_zero(bitmap *bitmap, int nbits)
{
	memset(bitmap, 0, BYTES(nbits));
	if (BITS(nbits))
		BYTE(bitmap, nbits) &= ~BITS(nbits);
}

static inline void bitmap_fill(bitmap *bitmap, int nbits)
{
	memset(bitmap, 0xff, BYTES(nbits));
	if (BITS(nbits))
		BYTE(bitmap, nbits) |= BITS(nbits);
}

static inline void bitmap_copy(bitmap *dst, bitmap *src, int nbits)
{
	memcpy(dst, src, BYTES(nbits));
	if (BITS(nbits)) {
		BYTE(dst, nbits) &= ~BITS(nbits);
		BYTE(dst, nbits) |= BYTE(src, nbits) & BITS(nbits);
	}
}

#define DEF_BINOP(_name, _op) \
	static inline void bitmap_##_name(bitmap *dst, bitmap *src1, bitmap *src2, \
					 int nbits) \
	{ \
		int n = 0; \
		while ((nbits - n) >= BITS_PER_LONG) { \
			LONG(dst, n) = LONG(src1, n) _op LONG(src2, n); \
			n += BITS_PER_LONG; \
		} \
		while ((nbits - n) >= 8) { \
			BYTE(dst, n) = BYTE(src1, n) _op BYTE(src2, n); \
			n += 8; \
		} \
		if (BITS(nbits)) { \
			BYTE(dst, nbits) &= ~BITS(nbits); \
			BYTE(dst, nbits) |= (BYTE(src1, nbits) _op BYTE(src2, nbits)) \
				& BITS(nbits); \
		} \
	}

DEF_BINOP(and, &)
DEF_BINOP(or, |)
DEF_BINOP(xor, ^)
DEF_BINOP(andnot, & ~)

#undef DEF_BINOP

static inline void bitmap_complement(bitmap *dst, bitmap *src, int nbits)
{
	int n = 0;

	while ((nbits - n) >= BITS_PER_LONG) {
		LONG(dst, n) = ~LONG(src, n);
		n += BITS_PER_LONG;
	}
	while ((nbits - n) >= 8) {
		BYTE(dst, n) = ~BYTE(src, n);
		n += 8;
	}
	if (BITS(nbits)) {
		BYTE(dst, nbits) &= ~BITS(nbits);
		BYTE(dst, nbits) |= ~BYTE(src, nbits) & BITS(nbits);
	}
}

static inline bool bitmap_equal(bitmap *src1, bitmap *src2, int nbits)
{
	if (memcmp(src1, src2, BYTES(nbits)) != 0)
		return false;
	if ((BYTE(src1, nbits) & BITS(nbits))
	    != (BYTE(src2, nbits) & BITS(nbits)))
		return false;
	return true;
}

static inline bool bitmap_intersects(bitmap *src1, bitmap *src2, int nbits)
{
	int n = 0;

	while ((nbits - n) >= BITS_PER_LONG) {
		if (LONG(src1, n) & LONG(src2, n))
			return true;
		n += BITS_PER_LONG;
	}
	while ((nbits - n) >= 8) {
		if (BYTE(src1, n) & BYTE(src2, n))
			return true;
		n += 8;
	}
	if (BITS(nbits) & BYTE(src1, nbits) & BYTE(src2, nbits)) {
		return true;
	}
	return false;
}

static inline bool bitmap_subset(bitmap *src1, bitmap *src2, int nbits)
{
	int n = 0;

	while ((nbits - n) >= BITS_PER_LONG) {
		if (LONG(src1, n) & ~LONG(src2, n))
			return false;
		n += BITS_PER_LONG;
	}
	while ((nbits - n) >= 8) {
		if (BYTE(src1, n) & ~BYTE(src2, n))
			return false;
		n += 8;
	}
	if (BITS(nbits) & (BYTE(src1, nbits) & ~BYTE(src2, nbits))) {
		return false;
	}
	return true;
}

static inline bool bitmap_full(bitmap *bitmap, int nbits)
{
	int n = 0;

	while ((nbits - n) >= BITS_PER_LONG) {
		if (LONG(bitmap, n) != -1UL)
			return false;
		n += BITS_PER_LONG;
	}
	while ((nbits - n) >= 8) {
		if (BYTE(bitmap, n) != 0xff)
			return false;
		n += 8;
	}
	if (BITS(nbits)
	    && ((BITS(nbits) & BYTE(bitmap, nbits)) != BITS(nbits))) {
		return false;
	}
	return true;
}

static inline bool bitmap_empty(bitmap *bitmap, int nbits)
{
	int n = 0;

	while ((nbits - n) >= BITS_PER_LONG) {
		if (LONG(bitmap, n))
			return false;
		n += BITS_PER_LONG;
	}
	while ((nbits - n) >= 8) {
		if (BYTE(bitmap, n))
			return false;
		n += 8;
	}
	if (BITS(nbits) && ((BITS(nbits) & BYTE(bitmap, nbits)))) {
		return false;
	}
	return true;
}


static inline bitmap *bitmap_alloc(int nbits)
{
	return malloc((nbits + 7) / 8);
}

#undef BYTE
#undef LONG
#undef BIT
#undef BYTES
#undef BITS

#endif /* CCAN_BITMAP_H_ */
