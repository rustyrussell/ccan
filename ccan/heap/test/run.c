#include <stdlib.h>
#include <stdio.h>

#include <ccan/heap/heap.h>
/* Include the C files directly. */
#include <ccan/heap/heap.c>
#include <ccan/tap/tap.h>

struct item {
	void *foobar;
	int v;
};

static bool heap_ok(const struct heap *h, heap_less_func_t less, int i)
{
	int l, r;

	l = 2 * i + 1;
	r = l + 1;

	if (l < h->len) {
		if (less(h->data[l], h->data[i])) {
			fprintf(stderr, "heap property violation\n");
			return false;
		}
		if (!heap_ok(h, less, l))
			return false;
	}
	if (r < h->len) {
		if (less(h->data[r], h->data[i])) {
			fprintf(stderr, "heap property violation\n");
			return false;
		}
		if (!heap_ok(h, less, r))
			return false;
	}
	return true;
}

static bool less(const struct item *a, const struct item *b)
{
	return a->v < b->v;
}

static bool __less(const void *a, const void *b)
{
	return less(a, b);
}

static bool more(const struct item *a, const struct item *b)
{
	return a->v > b->v;
}

static bool __more(const void *a, const void *b)
{
	return more(a, b);
}

static bool some_test(size_t n, bool is_less)
{
	struct item *items = calloc(n, sizeof(*items));
	struct item *item, *prev;
	struct heap *h;
	int i;

	if (items == NULL) {
		perror("items");
		exit(EXIT_FAILURE);
	}

	if (is_less)
		h = heap_init(__less);
	else
		h = heap_init(__more);
	if (h == NULL) {
		perror("heap_init");
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < n; i++) {
		item = &items[i];

		item->v = rand();
		printf("pushing %d\n", item->v);
		heap_push(h, item);
		if (!heap_ok(h, is_less ? __less : __more, 0))
			return false;
	}
	if (is_less) {
		heap_ify(h, __more);
		if (!heap_ok(h, __more, 0))
			return false;
		heap_ify(h, __less);
		if (!heap_ok(h, __less, 0))
			return false;
	} else {
		heap_ify(h, NULL);
		if (!heap_ok(h, __more, 0))
			return false;
	}

	for (i = 0; i < n; i++) {
		item = heap_pop(h);
		if (!heap_ok(h, is_less ? __less : __more, 0))
			return false;
		printf("popped %d\n", item->v);
		if (i > 0) {
			if (is_less) {
				if (less(item, prev))
					return false;
			} else {
				if (more(item, prev))
					return false;
			}
		}
		prev = item;
	}
	heap_free(h);
	free(items);
	return true;
}

int main(void)
{
	plan_tests(3);

	ok1(some_test(5000, true));
	ok1(some_test(1, true));
	ok1(some_test(33, false));

	return exit_status();
}
