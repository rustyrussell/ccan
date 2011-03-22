#include <ccan/failtest/failtest_override.h>
#include <ccan/talloc/talloc.c>
#include <stdbool.h>
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>

static unsigned my_malloc_count, my_free_count, my_realloc_count;

static void *my_malloc(size_t size)
{
	my_malloc_count++;
	return malloc(size);
}

static void my_free(void *ptr)
{
	my_free_count++;
	free(ptr);
}

static void *my_realloc(void *ptr, size_t size)
{
	my_realloc_count++;
	ok1(ptr);
	ok1(size);
	return realloc(ptr, size);
}

int main(int argc, char *argv[])
{
	int *p1, *p2;

	plan_tests(14);
	failtest_init(argc, argv);
	talloc_set_allocator(my_malloc, my_free, my_realloc);
	p1 = talloc_array(NULL, int, 10);
	if (!p1)
		failtest_exit(exit_status());
	ok1(my_malloc_count == 1);
	ok1(my_free_count == 0);
	ok1(my_realloc_count == 0);

	p2 = talloc_realloc(NULL, p1, int, 10000);
	if (!p2) {
		talloc_free(p1);
		failtest_exit(exit_status());
	}
	p1 = p2;
	ok1(my_malloc_count == 1);
	ok1(my_free_count == 0);
	ok1(my_realloc_count == 1);

	p2 = talloc(p1, int);
	if (!p2) {
		talloc_free(p1);
		failtest_exit(exit_status());
	}
	ok1(my_malloc_count == 2);
	ok1(my_free_count == 0);
	ok1(my_realloc_count == 1);

	talloc_free(p1);
	ok1(my_malloc_count == 2);
	ok1(my_free_count == 2);
	ok1(my_realloc_count == 1);

	failtest_exit(exit_status());
}
