#include <ccan/version/version.h>
#include <ccan/tap/tap.h>

int main(void)
{
	struct version a, b;

	plan_tests(26);

	/* cmp with normal versions */
	a = version(1, 0);
	b = version(2, 0);
	ok1(version_cmp(a, b) < 0);

	a = version(1, 1);
	ok1(version_cmp(a, b) < 0);

	a = version(2, 0);
	ok1(version_cmp(a, b) == 0);

	a = version(2, 1);
	ok1(version_cmp(a, b) > 0);

	b = version(2, 1);
	ok1(version_cmp(a, b) == 0);

	a = version(3, 0);
	ok1(version_cmp(a, b) > 0);

	a = version(3, 1);
	ok1(version_cmp(a, b) > 0);

	/* inline cmp */
	ok1(version_cmp(a, version(1, 0)) > 0);
	ok1(version_cmp(a, version(1, 1)) > 0);
	ok1(version_cmp(a, version(3, 0)) > 0);
	ok1(version_cmp(a, version(3, 1)) == 0);
	ok1(version_cmp(a, version(3, 2)) < 0);
	ok1(version_cmp(a, version(4, 0)) < 0);
	ok1(version_cmp(a, version(4, 1)) < 0);

	/* limits */
	a = version(0xFFFF, 0xFFFF);
	b = version(0xFFFE, 0xFFFF);
	ok1(version_cmp(a, b) > 0);
	ok1(version_cmp(b, a) < 0);

	b = version(0xFFFF, 0xFFFE);
	ok1(version_cmp(a, b) > 0);
	ok1(version_cmp(b, a) < 0);

	b = version(0xFFFF, 0xFFFF);
	ok1(version_cmp(a, b) == 0);
	ok1(version_cmp(b, a) == 0);

	b = version(0, 1);
	ok1(version_cmp(a, b) > 0);
	ok1(version_cmp(b, a) < 0);

	b = version(1, 0);
	ok1(version_cmp(a, b) > 0);
	ok1(version_cmp(b, a) < 0);

	b = version(0, 0);
	ok1(version_cmp(a, b) > 0);
	ok1(version_cmp(b, a) < 0);

	return exit_status();
}
