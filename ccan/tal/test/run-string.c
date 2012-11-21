#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *parent, *c;

	plan_tests(13);

	parent = tal(NULL, char);
	ok1(parent);

	c = tal_strdup(parent, "hello");
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(parent, "hello", 3);
	ok1(strcmp(c, "hel") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_typechk_(parent, char *);
	c = tal_dup(parent, char, "hello", 6, 0);
	ok1(strcmp(c, "hello") == 0);
	ok1(strcmp(tal_name(c), "char[]") == 0);
	ok1(tal_parent(c) == parent);

	/* Now with an extra byte. */
	c = tal_dup(parent, char, "hello", 6, 1);
	ok1(strcmp(c, "hello") == 0);
	ok1(strcmp(tal_name(c), "char[]") == 0);
	ok1(tal_parent(c) == parent);
	strcat(c, "x");

	c = tal_asprintf(parent, "hello %s", "there");
	ok1(strcmp(c, "hello there") == 0);
	ok1(tal_parent(c) == parent);
	tal_free(parent);

	return exit_status();
}
