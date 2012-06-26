#include <stdlib.h>
#include <stdio.h>

#include <ccan/talloc/talloc.h>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>

#include <ccan/rfc822/rfc822.h>

#include "helper.h"

/* failtest limitations mean we need these wrappers to test talloc
 * failure paths. */
static void *malloc_wrapper(size_t size)
{
	return malloc(size);
}

static void free_wrapper(void *ptr)
{
	free(ptr);
}

static void *realloc_wrapper(void *ptr, size_t size)
{
	return realloc(ptr, size);
}

#if 0
static void allocation_failure_exit(const char *s)
{
	fprintf(stderr, "Allocation failure: %s", s);
	exit(0);
}
#endif

static bool allocation_failed = false;

static void allocation_failure_continue(const char *s)
{
	fprintf(stderr, "Allocation failure: %s", s);
	allocation_failed = true;
}

void allocation_failure_check(void)
{
	if (allocation_failed) {
		fprintf(stderr, "Exiting due to earlier failed allocation\n");
		exit(0);
	}
}

void failtest_setup(int argc, char *argv[])
{
	failtest_init(argc, argv);
	rfc822_set_allocation_failure_handler(allocation_failure_continue);
	talloc_set_allocator(malloc_wrapper, free_wrapper, realloc_wrapper);
}
