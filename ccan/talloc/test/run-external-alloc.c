#include "talloc/talloc.c"
#include "tap/tap.h"
#include <assert.h>

static int ext_alloc_count, ext_free_count, ext_realloc_count;
static void *expected_parent;

static void *ext_alloc(void *parent, size_t size)
{
	ok1(parent == expected_parent);
	ext_alloc_count++;
	return malloc(size);
}

static void ext_free(void *ptr, void *parent)
{
	ok1(parent == expected_parent);
	ext_free_count++;
	free(ptr);
}

static void *ext_realloc(void *ptr, void *parent, size_t size)
{
	ok1(parent == expected_parent);
	ext_realloc_count++;
	return realloc(ptr, size);
}

int main(void)
{
	char *p, *p2, *head;
	plan_tests(10);

	talloc_external_enable(ext_alloc, ext_free, ext_realloc);
	head = talloc(NULL, char);
	assert(head);
	expected_parent = head;

	talloc_mark_external(head);

	p = talloc_array(head, char, 1);
	ok1(ext_alloc_count == 1);
	assert(p);

	/* Child is also externally allocated */
	expected_parent = p;
	p2 = talloc(p, char);
	ok1(ext_alloc_count == 2);

	expected_parent = head;
	p = talloc_realloc(NULL, p, char, 1000);
	ok1(ext_realloc_count == 1);
	assert(p);

	expected_parent = p;
	talloc_free(p2);
	ok1(ext_free_count == 1);

	expected_parent = head;
	talloc_free(p);
	ok1(ext_free_count == 2);

	return exit_status();
}
