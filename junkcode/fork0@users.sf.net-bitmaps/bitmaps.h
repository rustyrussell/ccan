/* bitmaps and bitmap operations */
#ifndef _BITMAPS_H_
#define _BITMAPS_H_

/*
 * Bitmaps are arrays of unsigned ints, filled with bits in most-
 * significant-first order. So bitmap[0] shall contain the bits from
 * 0 to 31 on a 32bit architecture, bitmap[1] - 32-63, and so forth.
 *
 * The callers are responsible to do all the bounds-checking.
 */
enum { BITS_PER_BITMAP_ELEM = 8 * sizeof(unsigned) };

typedef unsigned bitmap_elem_t;

/* returned is the unshifted bit state. IOW: NOT 0 or 1, but something
 * like 0x00001000 or 0x40000000 */
static inline
bitmap_elem_t bitmap_test_bit(const bitmap_elem_t* bits, unsigned bit)
{
    if ( sizeof(*bits) == 4 )
	return bits[bit >> 5] & (0x80000000 >> (bit & 0x1f));
    else if ( sizeof(*bits) == 8 )
	return bits[bit >> 6] & (0x8000000000000000ull >> (bit & 0x3f));
    else
    {
	return bits[bit / BITS_PER_BITMAP_ELEM] &
	    1 << ((BITS_PER_BITMAP_ELEM - bit % BITS_PER_BITMAP_ELEM) - 1);
    }
}

static inline
bitmap_elem_t bitmap_set_bit(bitmap_elem_t* bits, unsigned bit)
{
    if ( sizeof(*bits) == 4 )
	return bits[bit >> 5] |= (0x80000000 >> (bit & 0x1f));
    else if ( sizeof(*bits) == 8 )
	return bits[bit >> 6] |= (0x8000000000000000ull >> (bit & 0x3f));
    else
    {
	return bits[bit / BITS_PER_BITMAP_ELEM] |=
	    1 << ((BITS_PER_BITMAP_ELEM - bit % BITS_PER_BITMAP_ELEM) - 1);
    }
}

/* pos must position the bits inside of a bitmap element, otherwise
 * the index shift puts the bits in the wrong word (for simplicity).
 * Only low 8 bits of b8 shall be used */
static inline
void bitmap_set_8bits_fast(bitmap_elem_t* bits, unsigned pos, unsigned b8)
{
    if ( sizeof(*bits) == 4 )
	bits[pos >> 5] |= b8 << (24 - (pos & 0x1f));
    else if ( sizeof(*bits) == 8 )
	bits[pos >> 6] |= b8 << (56 - (pos & 0x3f));
    else
    {
	bits[pos / BITS_PER_BITMAP_ELEM] |=
	    b8 << (BITS_PER_BITMAP_ELEM - 8 - pos % BITS_PER_BITMAP_ELEM);
    }
}

static inline
bitmap_elem_t bitmap_clear_bit(bitmap_elem_t* bits, unsigned bit)
{
    if ( sizeof(*bits) == 4 )
	return bits[bit >> 5] &= ~(0x80000000 >> (bit & 0x1f));
    else if ( sizeof(*bits) == 8 )
	return bits[bit >> 6] &= ~(0x8000000000000000ull >> (bit & 0x3f));
    else
    {
	return bits[bit / BITS_PER_BITMAP_ELEM] &=
	    ~(1 << ((BITS_PER_BITMAP_ELEM - bit % BITS_PER_BITMAP_ELEM) - 1));
    }
}

#endif /* _BITMAPS_H_ */

