#include <ccan/likely/likely.c>
#include <ccan/likely/likely.h>
#include <ccan/tap/tap.h>
#include <stdlib.h>

static bool one_seems_likely(unsigned int val)
{
	if (likely(val == 1))
		return true;
	return false;
}

static bool one_seems_unlikely(unsigned int val)
{
	if (unlikely(val == 1))
		return true;
	return false;
}

static unlikely_func bool calling_is_unlikely(void)
{
	return true;
}

int main(int argc, char *argv[])
{
	plan_tests(5);

	/* Without debug, we can only check that it doesn't effect functions. */
	ok1(one_seems_likely(1));
	ok1(!one_seems_likely(2));
	ok1(one_seems_unlikely(1));
	ok1(!one_seems_unlikely(2));
	ok1(calling_is_unlikely());
	exit(exit_status());
}
