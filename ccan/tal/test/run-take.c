#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *parent, *c;

	plan_tests(15);

	parent = tal(NULL, char);
	ok1(parent);

	c = tal_strdup(parent, "hello");

	c = tal_strdup(TAL_TAKE, c);
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(TAL_TAKE, c, 5);
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(TAL_TAKE, c, 3);
	ok1(strcmp(c, "hel") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_dup(TAL_TAKE, char, c, 1, 0);
	ok1(c[0] == 'h');
	ok1(tal_parent(c) == parent);

	c = tal_dup(TAL_TAKE, char, c, 1, 2);
	ok1(c[0] == 'h');
	strcpy(c, "hi");
	ok1(tal_parent(c) == parent);

	/* No leftover allocations. */
	tal_free(c);
	ok1(tal_first(parent) == NULL);

	c = tal_strdup(parent, "hello %s");
	c = tal_asprintf(TAL_TAKE, c, "there");
	ok1(strcmp(c, "hello there") == 0);
	ok1(tal_parent(c) == parent);
	/* No leftover allocations. */
	tal_free(c);
	ok1(tal_first(parent) == NULL);

	tal_free(parent);

	return exit_status();
}
