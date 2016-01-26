#include "config.h"

#include <string.h>

#include <ccan/cppmagic/cppmagic.h>
#include <ccan/tap/tap.h>

static inline void check1(const char *orig, const char *expand,
			  const char *match)
{
	ok(strcmp(expand, match) == 0,
	   "%s => %s : %s", orig, expand, match);
}

#define CHECK1(orig, match) \
	check1(#orig, CPPMAGIC_STRINGIFY(orig), match)

int main(void)
{
	plan_tests(7);

	CHECK1(CPPMAGIC_NOTHING(), "");
	CHECK1(CPPMAGIC_GLUE2(a, b), "ab");

	CHECK1(CPPMAGIC_1ST(a), "a");
	CHECK1(CPPMAGIC_1ST(a, b), "a");
	CHECK1(CPPMAGIC_1ST(a, b, c), "a");

	CHECK1(CPPMAGIC_2ND(a, b), "b");
	CHECK1(CPPMAGIC_2ND(a, b, c), "b");

	/* This exits depending on whether all tests passed */
	return exit_status();
}
