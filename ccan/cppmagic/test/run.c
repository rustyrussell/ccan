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
	plan_tests(24);

	CHECK1(CPPMAGIC_NOTHING(), "");
	CHECK1(CPPMAGIC_GLUE2(a, b), "ab");

	CHECK1(CPPMAGIC_1ST(a), "a");
	CHECK1(CPPMAGIC_1ST(a, b), "a");
	CHECK1(CPPMAGIC_1ST(a, b, c), "a");

	CHECK1(CPPMAGIC_2ND(a, b), "b");
	CHECK1(CPPMAGIC_2ND(a, b, c), "b");

	CHECK1(CPPMAGIC_ISZERO(0), "1");
	CHECK1(CPPMAGIC_ISZERO(1), "0");
	CHECK1(CPPMAGIC_ISZERO(123), "0");
	CHECK1(CPPMAGIC_ISZERO(abc), "0");

	CHECK1(CPPMAGIC_NONZERO(0), "0");
	CHECK1(CPPMAGIC_NONZERO(1), "1");
	CHECK1(CPPMAGIC_NONZERO(123), "1");
	CHECK1(CPPMAGIC_NONZERO(abc), "1");

	CHECK1(CPPMAGIC_NONEMPTY(), "0");
	CHECK1(CPPMAGIC_NONEMPTY(0), "1");
	CHECK1(CPPMAGIC_NONEMPTY(a, b, c), "1");

	CHECK1(CPPMAGIC_ISEMPTY(), "1");
	CHECK1(CPPMAGIC_ISEMPTY(0), "0");
	CHECK1(CPPMAGIC_ISEMPTY(a, b, c), "0");
	
	CHECK1(CPPMAGIC_IFELSE(0)(abc)(def), "def");
	CHECK1(CPPMAGIC_IFELSE(1)(abc)(def), "abc");
	CHECK1(CPPMAGIC_IFELSE(not zero)(abc)(def), "abc");

	/* This exits depending on whether all tests passed */
	return exit_status();
}
