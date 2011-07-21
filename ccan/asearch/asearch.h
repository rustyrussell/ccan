/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_ASEARCH_H
#define CCAN_ASEARCH_H
#include <stdlib.h>
#include <ccan/typesafe_cb/typesafe_cb.h>

/**
 * asearch - search an array of elements
 * @key: pointer to item being searched for
 * @base: pointer to data to sort
 * @num: number of elements
 * @cmp: pointer to comparison function
 *
 * This function does a binary search on the given array.  The
 * contents of the array should already be in ascending sorted order
 * under the provided comparison function.
 *
 * Note that the key need not have the same type as the elements in
 * the array, e.g. key could be a string and the comparison function
 * could compare the string with the struct's name field.  However, if
 * the key and elements in the array are of the same type, you can use
 * the same comparison function for both sort() and asearch().
 */
#if HAVE_TYPEOF
#define asearch(key, base, num, cmp)					\
	((__typeof__(*(base))*)(bsearch((key), (base), (num), sizeof(*(base)), \
		typesafe_cb_cast(int (*)(const void *, const void *),	\
				 int (*)(const __typeof__(*(key)) *,	\
					 const __typeof__(*(base)) *),	\
				 (cmp)))))

#else
#define asearch(key, base, num, cmp)				\
	(bsearch((key), (base), (num), sizeof(*(base)),		\
		 (int (*)(const void *, const void *))(cmp)))
#endif

#endif /* CCAN_ASEARCH_H */
