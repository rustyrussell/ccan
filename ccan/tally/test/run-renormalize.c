#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct tally *tally = tally_new(2);

	plan_tests(4);
	tally->min = 0;
	tally->max = 0;
	tally->counts[0] = 1;

	/* This renormalize should do nothing. */
	renormalize(tally, 0, 1);
	ok1(tally->counts[0] == 1);
	ok1(tally->counts[1] == 0);
	tally->counts[1]++;

	/* This renormalize should collapse both into bucket 0. */
	renormalize(tally, 0, 3);
	ok1(tally->counts[0] == 2);
	ok1(tally->counts[1] == 0);

	free(tally);
	return exit_status();
}
