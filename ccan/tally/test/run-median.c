#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	int i;
	struct tally *tally = tally_new(100);
	ssize_t min, max, median;
	size_t err;

	max = (ssize_t)~(1ULL << (sizeof(max)*CHAR_BIT - 1));
	min = (ssize_t)(1ULL << (sizeof(max)*CHAR_BIT - 1));

	plan_tests(100*2 + 100*2 + 100*2);
	/* Simple median test: should always be around 0. */
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		tally_add(tally, -i);
		median = tally_approx_median(tally, &err);
		ok1(err <= 4);
		ok1(median - (ssize_t)err <= 0 && median + (ssize_t)err >= 0);
	}

	/* Works for big values too... */
	for (i = 0; i < 100; i++) {
		tally_add(tally, max - i);
		tally_add(tally, min + 1 + i);
		median = tally_approx_median(tally, &err);
		/* Error should be < 100th of max - min. */
		ok1(err <= max / 100 * 2);
		ok1(median - (ssize_t)err <= 0 && median + (ssize_t)err >= 0);
	}
	free(tally);

	tally = tally_new(10);
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		median = tally_approx_median(tally, &err);
		ok1(err <= i / 10 + 1);
		ok1(median - (ssize_t)err <= i/2
		    && median + (ssize_t)err >= i/2);
	}
	free(tally);

	return exit_status();
}
