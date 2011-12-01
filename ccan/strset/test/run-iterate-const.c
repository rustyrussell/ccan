#include <ccan/strset/strset.h>
#include <ccan/strset/strset.c>
#include <ccan/tap/tap.h>

static bool found = false;

/* Make sure const args work. */
static bool find_string(const char *str, const char *cmp)
{
	if (strcmp(str, cmp) == 0)
		found = true;
	return false;
}

int main(void)
{
	struct strset set;

	plan_tests(3);

	strset_init(&set);
	ok1(strset_set(&set, "hello"));
	ok1(strset_set(&set, "world"));
	strset_iterate(&set, find_string, (const char *)"hello");
	ok1(found);
	strset_destroy(&set);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
