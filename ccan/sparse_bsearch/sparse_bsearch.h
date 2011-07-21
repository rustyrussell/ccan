/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef SPARSE_BSEARCH_H
#define SPARSE_BSEARCH_H
#include <stdbool.h>
#include <stdlib.h>
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/check_type/check_type.h>

/**
 * sparse_bsearch - binary search of a sorted array with gaps.
 * @key: the key to search for
 * @base: the head of the array
 * @nmemb: the number of elements in the array
 * @cmpfn: the compare function (qsort/bsearch style)
 * @validfn: whether this element is valid.
 *
 * Binary search of a sorted array, which may have some invalid entries.
 * Note that cmpfn and validfn take const pointers.
 *
 * Example:
 *	static bool val_valid(const unsigned int *val)
 *	{
 *		return *val != 0;
 *	}
 *	static int val_cmp(const unsigned int *a, const unsigned int *b)
 *	{
 *		return (int)((*a) - (*b));
 *	}
 *	static unsigned int values[] = { 1, 7, 11, 1235, 99999 };
 *
 *	// Return true if this value is in set, and remove it.
 *	static bool remove_from_values(unsigned int val)
 *	{
 *		unsigned int *p;
 *		p = sparse_bsearch(&val, values, 5, val_cmp, val_valid);
 *		if (!p)
 *			return false;
 *		*p = 0;
 *		return true;
 *	}
 */
#define sparse_bsearch(key, base, nmemb, cmpfn, validfn)		\
	_sparse_bsearch((key)+check_types_match((key), &(base)[0]),	\
			(base), (nmemb), sizeof((base)[0]),		\
			typesafe_cb_cast(int (*)(const void *, const void *), \
					 int (*)(const __typeof__(*(base)) *, \
						 const __typeof__(*(base)) *), \
					 (cmpfn)),			\
			typesafe_cb_cast(bool (*)(const void *),	\
					 bool (*)(const __typeof__(*(base)) *), \
					 (validfn)))

void *_sparse_bsearch(const void *key, const void *base,
		      size_t nmemb, size_t size,
		      int (*cmpfn)(const void *, const void *),
		      bool (*validfn)(const void *));
#endif /* SPARSE_BSEARCH_H */
