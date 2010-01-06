#include <ccan/build_assert/build_assert.h>

int main(int argc, char *argv[])
{
#ifdef FAIL
	return EXPR_BUILD_ASSERT(1 == 0);
#else
	return 0;
#endif
}
