#include <ccan/rbtree/rbtree.c>
#include <ccan/tap/tap.h>
#include <ccan/talloc/talloc.h>
#include <string.h>
#include <stdbool.h>

#define NUM_ELEMS 10000

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

static bool insert_all(trbt_tree_t *rb, bool exist)
{
	unsigned int i;

	for (i = 0; i < NUM_ELEMS; i++) {
		int *p = trbt_insert32(rb, i, talloc_memdup(rb, &i, sizeof(i)));
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

static void delete_all(trbt_tree_t *rb)
{
	unsigned int i;

	for (i = 0; i < NUM_ELEMS; i++) {
		trbt_delete32(rb, i);
	}
}

int main(void)
{
	trbt_tree_t *rb;
	void *ctx = talloc_init("toplevel");

	plan_tests(7);

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

	/* This exits depending on whether all tests passed */
	return exit_status();
}
