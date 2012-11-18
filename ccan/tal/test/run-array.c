#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

int main(void)
{
	char *parent, *c[4];
	int i;

	plan_tests(9);

	parent = tal(NULL, char);
	ok1(parent);

	/* Zeroing allocations. */
	for (i = 0; i < 4; i++) {
		c[i] = talz(parent, char);
		ok1(*c[i] == '\0');
		tal_free(c[i]);
	}

	/* Array allocation. */
	for (i = 0; i < 4; i++) {
		c[i] = tal_arr(parent, char, 4);
		strcpy(c[i], "abc");
		tal_free(c[i]);
	}

	/* Zeroing array allocation. */
	for (i = 0; i < 4; i++) {
		c[i] = tal_arrz(parent, char, 4);
		ok1(!c[i][0] && !c[i][1] && !c[i][2] && !c[i][3]);
		strcpy(c[i], "abc");
		tal_free(c[i]);
	}
	tal_free(parent);

	return exit_status();
}
