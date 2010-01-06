#include <ccan/talloc/talloc.c>
#include <ccan/tap/tap.h>
#include <assert.h>

int main(void)
{
	char *c;
	int *i;

	plan_tests(12);

	/* Set C to a valid pointer, with correct parent. */
	talloc_set(&c, NULL);
	ok1(c >= (char *)(intptr_t)getpagesize());
	ok1(talloc_parent(c) == NULL);

	/* Free it, should blatt c. */
	talloc_free(c);
	ok1(c);
	ok1(c < (char *)(intptr_t)getpagesize());

	/* Same test, indirect. */
	talloc_set(&i, NULL);
	talloc_set(&c, i);
	ok1(c >= (char *)(intptr_t)getpagesize());
	ok1(i >= (int *)(intptr_t)getpagesize());
	ok1(talloc_parent(i) == NULL);
	ok1(talloc_parent(c) == i);
	talloc_free(i);
	ok1(c);
	ok1(c < (char *)(intptr_t)getpagesize());
	ok1(i);
	ok1(i < (int *)(intptr_t)getpagesize());

	return exit_status();
}
