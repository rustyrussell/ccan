#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	int i;
	struct tally *tally = tally_new(0);

	plan_tests(100 * 4);
	/* Test max, min and num. */
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		ok1(tally_num(tally) == i*2 + 1);
		tally_add(tally, -i);
		ok1(tally_num(tally) == i*2 + 2);
		ok1(tally_max(tally) == i);
		ok1(tally_min(tally) == -i);
	}
	free(tally);
	return exit_status();
}
