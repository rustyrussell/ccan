#include <ccan/build_assert/build_assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	plan_tests(1);
	ok1(EXPR_BUILD_ASSERT(1 == 1) == 0);
	return exit_status();
}
