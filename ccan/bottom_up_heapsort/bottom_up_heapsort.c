/* 3-clause BSD license - see LICENSE file for details */
#include <ccan/bottom_up_heapsort/bottom_up_heapsort.h>
/*
  Copyright (c) 2013,2015, Maxim Zakharov
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification,
  are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice, this
    list of conditions and the following disclaimer in the documentation and/or
    other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
  ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

/*
 * Swap two areas of size number of bytes.  Although qsort(3) permits random
 * blocks of memory to be sorted, sorting pointers is almost certainly the
 * common case (and, were it not, could easily be made so).  Regardless, it
 * isn't worth optimizing; the SWAP's get sped up by the cache, and pointer
 * arithmetic gets lost in the time required for comparison function calls.
 */
#define	SWAP(a, b, count, size, tmp) { \
	count = size; \
	do { \
		tmp = *a; \
		*a++ = *b; \
		*b++ = tmp; \
	} while (--count); \
}

/* Copy one block of size size to another. */
#define COPY(a, b, count, size, tmp1, tmp2) { \
	count = size; \
	tmp1 = a; \
	tmp2 = b; \
	do { \
		*tmp1++ = *tmp2++; \
	} while (--count); \
}

/*
   Modified version of BOTTOM-UP-HEAPSORT
   Ingo Wegener, BOTTOM-UP-HEAPSORT, a new variant of HEAPSORT beating,
   on an average, QUICKSORT (if n is not very small), Theoretical Computer Science 118 (1993),
   pp. 81-98, Elsevier; n >= 16000 for median-3 version of QUICKSORT

   The idea of delayed reheap after moving the root to its place is from
   D. Levendeas, C. Zaroliagis, Heapsort using Multiple Heaps, in Proc. 2nd Panhellenic
   Student Conference on Informatics -- EUREKA. – 2008. – P. 93–104.
   It saves (n-2)/2 swaps and (n-2)/2 comparisons, for n > 3.
*/

/* Search for the special leaf. */
#define LEAF_SEARCH(m, i, j) { \
	j = i; \
	while ((j << 1) < m) {	\
	    j <<= 1; \
	    if ((*compar)(base + j * size, base + size * (j + 1), arg) < 0) j++; \
	} \
	if ((j << 1) == m) j = m; \
    }

/* Find the place of a[i] in the path to the special leaf. */
#define BOTTOM_UP_SEARCH(i, j) {					\
	while(j > i && (*compar)(base + i * size, base + j * size, arg) > 0) { \
	    j >>= 1; \
	} \
    }

/* Rearrange the elements in the path. */
#define INTERCHANGE(i, j) { \
	COPY(k, base + j * size, cnt, size, tmp1, tmp2); \
	COPY(base + j * size, base + i * size, cnt, size, tmp1, tmp2); \
	while (i < j) { \
	    j >>= 1; \
	    p = base + j * size; \
	    t = k; \
	    SWAP(t, p, cnt, size, tmp);	\
	} \
    }

/* Bottom-up reheap procedure. */
#define BOTTOM_UP_REHEAP(m, i) { \
	LEAF_SEARCH(m, i, j); \
	BOTTOM_UP_SEARCH(i, j); \
	INTERCHANGE(i, j); \
    }

int
_bottom_up_heapsort(void *vbase, size_t nmemb, size_t size,
		    int (*compar)(const void *, const void *, void*), void *arg)
{
    size_t cnt, i, j;
    ssize_t l;
    char tmp, *tmp1, *tmp2;
    char *base, *k, *p, *t;

	if (nmemb <= 1)
		return (0);

	if (!size) {
		errno = EINVAL;
		return (-1);
	}

	if (compar == NULL) {
		errno = EINVAL;
		return (-2);
	}

	if ((k = malloc(size)) == NULL)
		return (-3);

	/*
	 * Items are numbered from 1 to nmemb, so offset from size bytes
	 * below the starting address.
	 */
	base = (char *)vbase - size;

	for (l = (nmemb >> 1); l > 0; l--) {
	    i = l;
	    BOTTOM_UP_REHEAP(nmemb, i);
	}

	/*
	 * For each element of the heap, leave the largest in its final slot,
	 * then recreate the heap.
	 */
	while (nmemb > 1) {
	    p = base + size;
	    t = base + size * nmemb;
	    SWAP(t, p, cnt, size, tmp);
	    --nmemb;
	    if (nmemb > 3) {
		p = base + (l = 2) * size;
		t = base + 3 * size;
		if ((*compar)(t, p, arg) > 0) {
		    p = t;
		    l = 3;
		}
		t = base + size * nmemb;
		SWAP(t, p, cnt, size, tmp);
		--nmemb;
		BOTTOM_UP_REHEAP(nmemb, l);
	    }
	    BOTTOM_UP_REHEAP(nmemb, 1);
	}
	free(k);
	return (0);
}


