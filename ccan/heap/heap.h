/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_HEAP_H
#define CCAN_HEAP_H

#include <stdbool.h>
#include <stdlib.h>

typedef bool (*heap_less_func_t)(const void *, const void *);

/**
 * struct heap - a simple, generic heap structure
 * @data: array of pointers to the heap's entries
 * @less: function to compare heap entries
 * @cap: capacity of the heap array in @data
 * @len: number of valid elements in the heap array
 *
 * The @less function determines the nature of the heap. If @less is
 * something akin to 'return a.foo < b.foo', then the heap will be
 * a min heap. Conversely, a '>' predicate will result in a max heap.
 *
 * Elements in the @data array are allocated as needed, hence the need for
 * @cap and @len.
 */
struct heap {
	void **data;
	heap_less_func_t less;
	size_t cap;
	size_t len;
};

/**
 * heap_init - allocate and initialise an empty heap
 * @less: function to be used to compare heap entries
 *
 * Returns a pointer to an initialised heap struct on success, NULL if
 * the heap could not be allocated.
 *
 * See also: HEAP_INIT()
 */
struct heap *heap_init(heap_less_func_t less);

/**
 * HEAP_INIT - initialiser for an empty heap
 * @func: comparison function to be used in the heap
 *
 * Explicit initialiser for a heap.
 *
 * See also: heap_init()
 */
#define HEAP_INIT(func) { NULL, func, 0, 0 }

/**
 * heap_free - free a heap allocated via heap_init()
 * @heap: the heap to be freed
 *
 * Note that this only frees the heap and its internal resources, not
 * the entries pointed to by it.
 *
 * See also: heap_init()
 */
void heap_free(struct heap *heap);

/**
 * heap_ify - enforce the heap property based on a new comparison function
 * @h: heap to be heapified
 * @less: new comparison function
 *
 * Complexity: O(n)
 */
void heap_ify(struct heap *h, heap_less_func_t less);

/**
 * heap_push - push a new heap entry
 * @h: heap to receive the new entry
 * @data: pointer to the new entry
 *
 * Returns 0 on success, -1 on error.
 *
 * Complexity: O(log n)
 *
 * See also: heap_pop()
 */
int heap_push(struct heap *h, void *data);

/**
 * heap_pop - pops the root heap entry
 * @h: heap to pop the head from
 *
 * Returns the root entry of the heap after extracting it, or NULL on error.
 *
 * Note: Calling heap_pop() on an empty heap is a bug. When in doubt,
 * check heap->len. See heap_peek()'s documentation for an example.
 *
 * Complexity: O(log n)
 *
 * See also: heap_push(), heap_peek()
 */
void *heap_pop(struct heap *h);

/**
 * heap_peek - inspect the root entry of a heap
 * @h: heap whose root entry is to be inspected
 *
 * Returns the root entry in the heap, without extracting it from @h.
 *
 * Note: Calling heap_peek() on an empty heap is a bug; check the heap's
 * number of items and act accordingly, as in the example below.
 *
 * See also: heap_pop()
 *
 * Example:
 *	static inline void *heap_peek_safe(const struct heap *h)
 *	{
 *		return h->len ? heap_peek(h) : NULL;
 *	}
 */
static inline void *heap_peek(const struct heap *h)
{
	return h->data[0];
}

#endif /* CCAN_HEAP_H */
