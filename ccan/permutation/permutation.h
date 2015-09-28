/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_PERMUTATION_H
#define CCAN_PERMUTATION_H

#include <config.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/mem/mem.h>

/*
 * We limit the number of elements to 64, to keep our data structures
 * neat.  That seems low, but even 64! permutations is far too many to
 * really generate in practice.
 */
#define PERMUTATION_MAX_SHIFT	6
#define PERMUTATION_MAX_ITEMS	(1 << PERMUTATION_MAX_SHIFT)

/**
 * struct permutation - represents a permutation
 *
 * The fields here are user visible, but it's allocated internally
 * with extra state information appended.
 *
 * @n: Number of elements in the permutation
 * @v: values 0..(n-1) arranged in current permutation
 */
struct permutation {
	int n;
	uint8_t v[];
	/* Private state data follows */
};


/**
 * permutation_count - determine number of permutations
 * @n: Number of elements
 *
 * Returns the number of permutations of @n elements.
 */
unsigned long long permutation_count(int n);

/**
 * permutation_swap_t - represents a swap of two items
 *
 * Encodes a swap of two elements in an array.  Should be treated as
 * opaque, except that 0 always represents "swap nothing".
 */
typedef unsigned permutation_swap_t;


/**
 * permutation_swap_a - first item to swap
 * permutation_swap_b - second item to swap
 */
static inline int permutation_swap_a(permutation_swap_t swap)
{
	return swap % PERMUTATION_MAX_ITEMS;
}

static inline int permutation_swap_b(permutation_swap_t swap)
{
	return swap / PERMUTATION_MAX_ITEMS;
}

/**
 * permutation_swap - swap two elements in an array
 * @base: address of array
 * @size: size of each array element
 * @swap: which elements to swap
 *
 * Swaps the elements at index permutation_swap_a(@swap) and
 * permutation_swap_b(@swap) in the array at @base of items of size
 * @size.
 */
static inline void permutation_swap_mem(void *base, size_t size,
					permutation_swap_t swap)
{
	char *ap = (char *)base + (permutation_swap_a(swap) * size);
	char *bp = (char *)base + (permutation_swap_b(swap) * size);

	BUILD_ASSERT(sizeof(permutation_swap_t) * 8
		     >= PERMUTATION_MAX_SHIFT * 2);

	if (!swap)
		return;

	memswap(ap, bp, size);
}

/**
 * PERMUTATION_SWAP - swap two elements in an array
 * @a_: array
 * @swap_: which elements to swap
 *
 * As permutation_swap(), but must act on a C array declared at the
 * right size.
 */
#define PERMUTATION_SWAP(a_, swap_)					\
	do {								\
		permutation_swap((a_), sizeof((a_)[0]), (swap_));	\
	} while (0)


/**
 * permutation_new - allocates a new identity permutation
 * @n: Number of elements
 *
 * Allocates and initializes a new permutation of @n elements.
 * Initially it represents the identity permutation.
 */
struct permutation *permutation_new(int n);

/**
 * PERMUTATION_NEW - allocates a new identity permutation of an array
 * @array_: Array to permute
 *
 * As permutation_new() but take the number of elements from the
 * declaration of @array_.
 */
#define PERMUTATION_NEW(array_)	(permutation_new(ARRAY_SIZE(array_)))

/**
 * permutation_change - Advance to a new permutation
 * @pi: Current permutation
 *
 * Advances @pi to the next permutation by the plain changes method
 * (Steinhaus-Johnson-Trotter algorithm).
 *
 * Returns the elements which were swapped in @pi, or 0 if there are
 * no more permutations.
 */
permutation_swap_t permutation_change(struct permutation *pi);

/**
 * permutation_change_array - Advance an array to a new permutation
 * @pi: Current permutation
 * @base: Address of array
 * @size: Size of array elements
 *
 * Assuming the array at @base is currently in permutation @pi,
 * advances @pi to the next permutation (as permutation_change()) and
 * keeps the array in sync.
 *
 * Returns true if the permutation was advanced, false if there are no
 * more permutations.
 */
static inline bool permutation_change_array(struct permutation *pi,
					    void *base, size_t size)
{
	permutation_swap_t swap = permutation_change(pi);

	permutation_swap_mem(base, size, swap);
	return (swap != 0);
}

static inline bool permutation_change_array_check_(struct permutation *pi,
						   void *base, size_t size,
						   int n)
{
	assert(n == pi->n);
	return permutation_change_array(pi, base, size);
}

/**
 * PERMUTATION_CHANGE_ARRAY - Advance an array to a new permutation
 * @pi_: Current permutation
 * @a_: Array
 *
 * As permutation_change_array(), but operate on array @a_, which must
 * be a C array declared with the same number of elements as @pi_.
 */
#define PERMUTATION_CHANGE_ARRAY(pi_, a_)				\
	(permutation_change_array_check_((pi_), (a_),			\
					 sizeof((a_)[0]), ARRAY_SIZE(a_)))

#endif /* CCAN_PERMUTATION_H */
