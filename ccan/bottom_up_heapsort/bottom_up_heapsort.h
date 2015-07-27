/* 3-clause BSD license - see LICENSE file for details */
#ifndef CCAN_BOTTOM_UP_HEAPSORT_H
#define CCAN_BOTTOM_UP_HEAPSORT_H
#include "config.h"
#include <ccan/order/order.h>
#include <stddef.h>

/**
 * bottom_up_heapsort - sort an array of elements
 * @base: pointer to data to sort
 * @num: number of elements
 * @cmp: pointer to comparison function
 * @ctx: a context pointer for the cmp function
 *
 * This function does a sort on the given array.  The resulting array
 * will be in ascending sorted order by the provided comparison function.
 *
 * The @cmp function should exactly match the type of the @base and
 * @ctx arguments.  Otherwise it can take three const void *.
 */
#define bottom_up_heapsort(base, num, cmp, ctx)				\
    _bottom_up_heapsort((base), (num), sizeof(*(base)),			\
			total_order_cast((cmp), *(base), (ctx)), (ctx))

int
_bottom_up_heapsort(void *base, size_t nmemb, size_t size,
		    int (*compar)(const void *, const void *, void*), void *arg);


#endif /* CCAN_BOTTOM_UP_HEAPSORT_H */
