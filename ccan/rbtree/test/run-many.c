#include <ccan/rbtree/rbtree.c>
#include <ccan/tap/tap.h>
#include <ccan/talloc/talloc.h>
#include <string.h>
#include <stdbool.h>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#define NUM_ELEMS 100

/* We want to test talloc failure paths. */
static void *my_malloc(size_t size)
{
	return malloc(size);
}

static void my_free(void *ptr)
{
	free(ptr);
}

static void *my_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

static bool lookup_all(trbt_tree_t *rb, bool exist)
{
	unsigned int i;

	for (i = 0; i < NUM_ELEMS; i++) {
		int *p = trbt_lookup32(rb, i);
		if (p) {
			if (!exist)
				return false;
			if (*p != i)
				return false;
		} else
			if (exist)
				return false;
	}
	return true;
}

static bool add_one(trbt_tree_t *rb, bool exist, unsigned int i)
{
	int *new = talloc_memdup(rb, &i, sizeof(i));
	int *p;

	if (!new)
		return false;

	p = trbt_insert32(rb, i, new);
	if (p) {
		if (!exist)
			return false;
		if (*p != i)
			return false;
	} else {
		if (exist)
			return false;
		else
			if (!trbt_lookup32(rb, i))
				return false;
	}
	return true;
}

static bool insert_all(trbt_tree_t *rb, bool exist)
{
	unsigned int i;

	for (i = 0; i < NUM_ELEMS; i++) {
		if (!add_one(rb, exist, i))
			return false;
	}
	return true;
}

static void delete_all(trbt_tree_t *rb)
{
	unsigned int i;

	/* Don't delete them in the obvious order. */
	for (i = 0; i < NUM_ELEMS / 2; i++) {
		trbt_delete32(rb, i);
	}

	for (i = NUM_ELEMS-1; i >= NUM_ELEMS / 2; i--) {
		trbt_delete32(rb, i);
	}
}

static void *ctx;
static trbt_tree_t *rb;

static void exit_test(void)
{
	talloc_free(rb);
	ok1(talloc_total_blocks(ctx) == 1);
	talloc_free(ctx);
	failtest_exit(exit_status());
}

int main(int argc, char *argv[])
{
	failtest_init(argc, argv);
	tap_fail_callback = exit_test;
	plan_tests(8);

	ctx = talloc_strdup(NULL, "toplevel");

	talloc_set_allocator(my_malloc, my_free, my_realloc);

	rb = trbt_create(ctx, 0);
	ok1(rb);

	/* None should be there. */
	ok1(lookup_all(rb, false));

	/* Insert, none should be there previously. */
	ok1(insert_all(rb, false));

	/* All there now. */
	ok1(lookup_all(rb, true));

	/* Replace all. */
	ok1(insert_all(rb, true));

	/* Delete all. */
	delete_all(rb);

	/* One more time... */
	ok1(lookup_all(rb, false));
	ok1(insert_all(rb, false));

	/* All are children of rb, so this is clean. */
	talloc_free(rb);

	/* No memory leaks? */
	ok1(talloc_total_blocks(ctx) == 1);
	talloc_free(ctx);

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
