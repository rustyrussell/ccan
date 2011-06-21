#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	int i;
	struct tally *tally = tally_new(0);
	ssize_t min, max;

	max = (ssize_t)~(1ULL << (sizeof(max)*CHAR_BIT - 1));
	min = (ssize_t)(1ULL << (sizeof(max)*CHAR_BIT - 1));

	plan_tests(100 + 100);
	/* Simple mean test: should always be 0. */
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		tally_add(tally, -i);
		ok1(tally_mean(tally) == 0);
	}

	/* Works for big values too... */
	for (i = 0; i < 100; i++) {
		tally_add(tally, max - i);
		tally_add(tally, min + 1 + i);
		ok1(tally_mean(tally) == 0);
	}

	free(tally);
	return exit_status();
}
