#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *parent, *c;

	plan_tests(9);

	parent = tal(NULL, char);
	ok1(parent);

	c = tal_strdup(parent, "hello");
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(parent, "hello", 3);
	ok1(strcmp(c, "hel") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_memdup(parent, "hello", 6);
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_asprintf(parent, "hello %s", "there");
	ok1(strcmp(c, "hello there") == 0);
	ok1(tal_parent(c) == parent);
	tal_free(parent);

	return exit_status();
}
