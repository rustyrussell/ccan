/* GNU LGPL version 2 (or later) - see LICENSE file for details */
#ifndef CCAN_BITMAP_TAL_H
#define CCAN_BITMAP_TAL_H
#include <ccan/tal/tal.h>
#include <ccan/bitmap/bitmap.h>

static inline bitmap *bitmap_tal(const tal_t* ctx, unsigned long nbits)
{
    return tal_arr(ctx, bitmap, BITMAP_NWORDS(nbits));
}

static inline bitmap *bitmap_talz(const tal_t* ctx, unsigned long nbits)
{
    return tal_arrz(ctx, bitmap, BITMAP_NWORDS(nbits));
}

static inline bitmap *bitmap_tal_fill(const tal_t* ctx, unsigned long nbits)
{
    bitmap *nbitmap = tal_arr(ctx, bitmap, BITMAP_NWORDS(nbits));

    if (nbitmap)
        bitmap_fill(nbitmap, nbits);
    return nbitmap;
}

static inline bool bitmap_tal_resizez(bitmap **bitmap,
				      unsigned long obits,
				      unsigned long nbits)
{
    if (!tal_resize(bitmap, BITMAP_NWORDS(nbits)))
	return false;

    if (nbits > obits)
        bitmap_zero_range(*bitmap, obits, nbits);

    return true;
}

static inline bool bitmap_tal_resize_fill(bitmap **bitmap,
					  unsigned long obits,
					  unsigned long nbits)
{
    if (!tal_resize(bitmap, BITMAP_NWORDS(nbits)))
	return false;

    if (nbits > obits)
        bitmap_fill_range(*bitmap, obits, nbits);

    return true;
}
#endif /* CCAN_BITMAP_TAL_H */
