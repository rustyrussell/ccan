#include <ccan/failtest/failtest.h>
#include <ccan/rbtree/rbtree.c>
#include <ccan/tap/tap.h>
#include <ccan/talloc/talloc.h>
#include <string.h>

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

static void *insert_callback(void *param, void *data)
{
	ok1(data == param);
	return talloc_strdup(NULL, "insert_callback");
}

int main(void)
{
	trbt_tree_t *rb;
	void *ctx = talloc_strdup(NULL, "toplevel");
	char *data, *data2;

	/* This is how many tests you plan to run */
	plan_tests(19);

	talloc_set_allocator(my_malloc, my_free, my_realloc);

	rb = trbt_create(ctx, 0);
	ok1(rb);
	ok1(talloc_is_parent(rb, ctx));

	/* Failed lookup. */
	ok1(trbt_lookup32(rb, 0) == NULL);
	ok1(trbt_lookup32(rb, -1) == NULL);

	/* Insert, should steal node onto data. */
	data = talloc_strdup(NULL, "data");
	ok1(trbt_insert32(rb, 0, data) == NULL);
	ok1(trbt_lookup32(rb, 0) == data);
	ok1(trbt_lookup32(rb, -1) == NULL);

	/* Thus, freeing the data will delete the node. */
	talloc_free(data);
	ok1(trbt_lookup32(rb, 0) == NULL);

	/* Try again. */
	data = talloc_strdup(NULL, "data");
	ok1(trbt_insert32(rb, 0, data) == NULL);

	/* Another insert should return old one. */
	data2 = talloc_strdup(NULL, "data2");
	ok1(trbt_insert32(rb, 0, data2) == data);
	ok1(trbt_lookup32(rb, 0) == data2);

	/* Freeing old data has no effect. */
	talloc_free(data);
	ok1(trbt_lookup32(rb, 0) == data2);

	/* Insert with callback on non-existing. */
	trbt_insert32_callback(rb, 1, insert_callback, NULL);
	ok1(strcmp(trbt_lookup32(rb, 1), "insert_callback") == 0);
	/* Insert with callback on existing. */
	trbt_insert32_callback(rb, 0, insert_callback, data2);
	ok1(strcmp(trbt_lookup32(rb, 0), "insert_callback") == 0);
	talloc_free(data2);

	/* Delete. */
	data2 = trbt_lookup32(rb, 1);
	trbt_delete32(rb, 1);
	ok1(trbt_lookup32(rb, 1) == NULL);
	ok1(trbt_lookup32(rb, 0));
	talloc_free(data2);

	/* This should free everything. */
	talloc_free(trbt_lookup32(rb, 0));
	talloc_free(rb);

	/* No memory leaks? */
	ok1(talloc_total_blocks(ctx) == 1);
	talloc_free(ctx);

	/* This exits depending on whether all tests passed */
	failtest_exit(exit_status());
}
