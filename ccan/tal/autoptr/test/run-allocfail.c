#include <ccan/tal/autoptr/autoptr.h>
#include <ccan/tal/autoptr/autoptr.c>
#include <ccan/tap/tap.h>

static unsigned int alloc_count, fail_after, error_count;

static void *failing_alloc(size_t size)
{
	if (alloc_count++ >= fail_after) {
		/* Make subsequent allocations succeed again. */
		alloc_count = 0;
		return NULL;
	}
	return malloc(size);
}

static void counting_error(const char *msg UNNEEDED)
{
	error_count++;
}

int main(void)
{
	char *ctx, *p, *sentinel = (char *)&sentinel, *ptr;

	plan_tests(11);
	tal_set_backend(failing_alloc, NULL, NULL, counting_error);

	/* Let setup allocations succeed. */
	fail_after = 1000;
	ctx = tal(NULL, char);
	p = tal(NULL, char);

	/* Fail the autonull struct allocation itself. */
	fail_after = 0;
	error_count = 0;
	ptr = sentinel;
	ok1(autonull_set_ptr(ctx, &ptr, p) == NULL);
	ok1(error_count == 1);
	ok1(ptr == sentinel);

	/* Fail the destructor2 registration. */
	fail_after = 1;
	error_count = 0;
	ok1(autonull_set_ptr(ctx, &ptr, p) == NULL);
	ok1(error_count == 1);
	ok1(ptr == sentinel);

	/* Fail the destructor registration: destructor2 must be unwound,
	 * so freeing p leaves ptr alone (and doesn't UAF). */
	fail_after = 2;
	error_count = 0;
	ok1(autonull_set_ptr(ctx, &ptr, p) == NULL);
	ok1(error_count == 1);
	tal_free(p);
	ok1(ptr == sentinel);

	/* Now succeed. */
	p = tal(NULL, char);
	fail_after = 4;
	ok1(autonull_set_ptr(ctx, &ptr, p) != NULL);
	ok1(ptr == p);

	tal_free(ctx);
	tal_free(p);
	tal_cleanup();
	return exit_status();
}
