/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/heap/heap.h>

/*
 * Allocating memory in chunks greater than needed does not yield measurable
 * speedups of the test program when linking against glibc 2.15.
 *
 * When the data array has to be shrunk though, limiting calls to realloc
 * does help a little bit (~7% speedup), hence the following parameter.
 */
#define HEAP_MEM_HYSTERESIS	4096

static inline void swap(struct heap *h, size_t i, size_t j)
{
	void *foo = h->data[i];

	h->data[i] = h->data[j];
	h->data[j] = foo;
}

static void __up(struct heap *h, size_t j)
{
	size_t i; /* parent */

	while (j) {
		i = (j - 1) / 2;

		if (h->less(h->data[j], h->data[i])) {
			swap(h, i, j);
			j = i;
		} else {
			break;
		}
	}
}

int heap_push(struct heap *h, void *data)
{
	if (h->len == h->cap) {
		void *m = realloc(h->data, (h->cap + 1) * sizeof(void *));
		if (m == NULL)
			return -1;
		h->data = m;
		h->cap++;
	}
	h->data[h->len++] = data;
	__up(h, h->len - 1);
	return 0;
}

static void __down(struct heap *h, size_t i)
{
	size_t l, r, j; /* left, right, min child */

	while (1) {
		l = 2 * i + 1;
		if (l >= h->len)
			break;
		r = l + 1;
		if (r >= h->len)
			j = l;
		else
			j = h->less(h->data[l], h->data[r]) ? l : r;

		if (h->less(h->data[j], h->data[i])) {
			swap(h, i, j);
			i = j;
		} else {
			break;
		}
	}
}

void *heap_pop(struct heap *h)
{
	void *ret = h->data[0];
	void *m;

	swap(h, 0, --h->len);
	if (h->len) {
		__down(h, 0);
		if (h->len == h->cap - HEAP_MEM_HYSTERESIS) {
			m = realloc(h->data, h->len * sizeof(void *));
			if (m == NULL)
				return NULL;
			h->data = m;
			h->cap = h->len;
		}
	}

	return ret;
}

struct heap *heap_init(heap_less_func_t less)
{
	struct heap *heap = calloc(1, sizeof(*heap));

	if (heap == NULL)
		return NULL;
	heap->less = less;
	return heap;
}

void heap_ify(struct heap *h, heap_less_func_t less)
{
	int i;

	if (less)
		h->less = less;

	for (i = h->len / 2 - 1; i >= 0; i--)
		__down(h, i);
}

void heap_free(struct heap *heap)
{
	free(heap->data);
	free(heap);
}
