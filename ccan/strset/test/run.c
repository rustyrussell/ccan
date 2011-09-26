#include <ccan/strset/strset.h>
#include <ccan/strset/strset.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct strset set;
	const char str[] = "hello";
	const char none[] = "";
	char *dup = strdup(str);

	/* This is how many tests you plan to run */
	plan_tests(24);

	strset_init(&set);

	ok1(!strset_test(&set, str));
	ok1(!strset_test(&set, none));
	ok1(!strset_clear(&set, str));
	ok1(!strset_clear(&set, none));

	ok1(strset_set(&set, str));
	ok1(strset_test(&set, str));
	/* We compare the string, not the pointer. */
	ok1(strset_test(&set, dup));
	ok1(!strset_test(&set, none));

	/* Delete should return original string. */
	ok1(strset_clear(&set, dup) == str);
	ok1(!strset_test(&set, str));
	ok1(!strset_test(&set, none));

	/* Try insert and delete of empty string. */
	ok1(strset_set(&set, none));
	ok1(strset_test(&set, none));
	ok1(!strset_test(&set, str));

	/* Delete should return original string. */
	ok1(strset_clear(&set, "") == none);
	ok1(!strset_test(&set, str));
	ok1(!strset_test(&set, none));

	/* Both at once... */
	ok1(strset_set(&set, none));
	ok1(strset_set(&set, str));
	ok1(strset_test(&set, str));
	ok1(strset_test(&set, none));
	ok1(strset_clear(&set, "") == none);
	ok1(strset_clear(&set, dup) == str);

	ok1(set.u.n == NULL);
	free(dup);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
