#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *parent, *c;

	plan_tests(24);

	/* We can take NULL. */
	ok1(take(NULL) == NULL);
	ok1(taken(NULL)); /* Undoes take() */
	ok1(!taken(NULL));

	parent = tal(NULL, char);
	ok1(parent);

	ok1(take(parent) == parent);
	ok1(taken(parent)); /* Undoes take() */
	ok1(!taken(parent));

	c = tal_strdup(parent, "hello");

	c = tal_strdup(parent, take(c));
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(parent, take(c), 5);
	ok1(strcmp(c, "hello") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_strndup(parent, take(c), 3);
	ok1(strcmp(c, "hel") == 0);
	ok1(tal_parent(c) == parent);

	c = tal_dup(parent, char, take(c), 1, 0);
	ok1(c[0] == 'h');
	ok1(tal_parent(c) == parent);

	c = tal_dup(parent, char, take(c), 1, 2);
	ok1(c[0] == 'h');
	strcpy(c, "hi");
	ok1(tal_parent(c) == parent);

	/* dup must reparent child. */
	c = tal_dup(NULL, char, take(c), 1, 0);
	ok1(c[0] == 'h');
	ok1(tal_parent(c) == NULL);

	/* No leftover allocations. */
	tal_free(c);
	ok1(tal_first(parent) == NULL);

	c = tal_strdup(parent, "hello %s");
	c = tal_asprintf(parent, take(c), "there");
	ok1(strcmp(c, "hello there") == 0);
	ok1(tal_parent(c) == parent);
	/* No leftover allocations. */
	tal_free(c);
	ok1(tal_first(parent) == NULL);

	tal_free(parent);
	ok1(!taken_any());

	return exit_status();
}
