#include <ccan/minmax/minmax.h>
#include <ccan/tap/tap.h>

int main(void)
{
	int a, b;

	/* This is how many tests you plan to run */
	plan_tests(23);

	ok1(min(1, 2) == 1);
	ok1(max(1, 2) == 2);
	ok1(min(-1, 1) == -1);
	ok1(max(-1, 1) == 1);

	ok1(min(-1U, 1U) == 1U);
	ok1(max(-1U, 1U) == -1U);

	ok1(max_t(signed int, -1, 1U) == 1);
	ok1(max_t(unsigned int, -1, 1) == -1U);

	ok1(min_t(signed int, -1, 1U) == -1);
	ok1(min_t(unsigned int, -1, 1) == 1U);

	ok1(clamp(1, 2, 5) == 2);
	ok1(clamp(2, 2, 5) == 2);
	ok1(clamp(3, 2, 5) == 3);
	ok1(clamp(5, 2, 5) == 5);
	ok1(clamp(6, 2, 5) == 5);

	ok1(clamp(-1, 2, 5) == 2);
	ok1(clamp(-1U, 2U, 5U) == 5U);

	ok1(clamp_t(signed int, -1, 2, 5) == 2);
	ok1(clamp_t(unsigned int, -1, 2, 5) == 5);

	/* test for double evaluation */
	a = b = 0;
	ok1(min(a++, b++) == 0);
	ok1((a == 1) && (b == 1));
	ok1(max(++a, ++b) == 2);
	ok1((a == 2) && (b == 2));

	/* This exits depending on whether all tests passed */
	return exit_status();
}
