/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef ALLOC_H
#define ALLOC_H
#include <stdio.h>
#include <stdbool.h>

/**
 * alloc_init - initialize a pool of memory for the allocator.
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 *
 * This stores all the setup state required to perform allocation within the
 * pool (there is no external state).  Any previous contents of @pool is
 * discarded.
 *
 * The same @pool and @poolsize arguments must be handed to the other alloc
 * functions after this.
 *
 * If the pool is too small for meaningful allocations, alloc_get will fail.
 *
 * Example:
 *	void *pool = malloc(32*1024*1024);
 *	if (!pool)
 *		err(1, "Failed to allocate 32MB");
 *	alloc_init(pool, 32*1024*1024);
 */
void alloc_init(void *pool, unsigned long poolsize);

/**
 * alloc_get - allocate some memory from the pool
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 * @size: the size of the desired allocation
 * @align: the alignment of the desired allocation (0 or power of 2)
 *
 * This is "malloc" within an initialized pool.
 *
 * It will return a unique pointer within the pool (ie. between @pool
 * and @pool+@poolsize) which meets the alignment requirements of
 * @align.  Note that the alignment is relative to the start of the pool,
 * so of @pool is not aligned, the pointer won't be either.
 *
 * Returns NULL if there is no contiguous room.
 *
 * Example:
 *	#include <ccan/alignof/alignof.h>
 *	...
 *		double *d = alloc_get(pool, 32*1024*1024,
 *				      sizeof(*d), ALIGNOF(*d));
 *		if (!d)
 *			err(1, "Failed to allocate a double");
 */
void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align);

/**
 * alloc_free - free some allocated memory from the pool
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 * @p: the non-NULL pointer returned from alloc_get.
 *
 * This is "free" within an initialized pool.  A pointer should only be
 * freed once, and must be a pointer returned from a successful alloc_get()
 * call.
 *
 * Example:
 *	alloc_free(pool, 32*1024*1024, d);
 */
void alloc_free(void *pool, unsigned long poolsize, void *free);

/**
 * alloc_size - get the actual size allocated by alloc_get
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 * @p: the non-NULL pointer returned from alloc_get.
 *
 * alloc_get() may overallocate, in which case you may use the extra
 * space exactly as if you had asked for it.
 *
 * The return value will always be at least the @size passed to alloc_get().
 *
 * Example:
 *	printf("Allocating a double actually got me %lu bytes\n",
 *		alloc_size(pool, 32*1024*1024, d));
 */
unsigned long alloc_size(void *pool, unsigned long poolsize, void *p);

/**
 * alloc_check - check the integrity of the allocation pool
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 *
 * alloc_check() can be used for debugging suspected pool corruption.  It may
 * be quite slow, but provides some assistance for hard-to-find overruns or
 * double-frees.  Unlike the rest of the code, it will not crash on corrupted
 * pools.
 *
 * There is an internal function check_fail() which this calls on failure which
 * is useful for placing breakpoints and gaining more insight into the type
 * of the corruption detected.
 *
 * Example:
 *	#include <assert.h>
 *	...
 *		assert(alloc_check(pool, 32*1024*1024));
 */
bool alloc_check(void *pool, unsigned long poolsize);

/**
 * alloc_visualize - dump information about the allocation pool
 * @pool: the contiguous bytes for the allocator to use
 * @poolsize: the size of the pool
 *
 * When debugging the allocator itself, it's often useful to see how
 * the pool is being used.  alloc_visualize() does that, but makes
 * assumptions about correctness (like the rest of the code) so if you
 * suspect corruption call alloc_check() first.
 *
 * Example:
 *	d = alloc_get(pool, 32*1024*1024, sizeof(*d), ALIGNOF(*d));
 *	if (!d) {
 *		fprintf(stderr, "Allocation failed!\n");
 *		if (!alloc_check(pool, 32*1024*1024))
 *			errx(1, "Allocation pool is corrupt");
 *		alloc_visualize(stderr, pool, 32*1024*1024);
 *		exit(1);
 *	}
 */
void alloc_visualize(FILE *out, void *pool, unsigned long poolsize);
#endif /* ALLOC_H */
