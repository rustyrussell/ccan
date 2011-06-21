#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	int i;
	struct tally *tally = tally_new(100);
	ssize_t min, max, mode;
	size_t err;

	max = (ssize_t)~(1ULL << (sizeof(max)*CHAR_BIT - 1));
	min = (ssize_t)(1ULL << (sizeof(max)*CHAR_BIT - 1));

	plan_tests(100 + 50 + 100 + 100 + 10);
	/* Simple mode test: should always be around 0 (we add that twice). */
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		tally_add(tally, -i);
		mode = tally_approx_mode(tally, &err);
		if (i < 50)
			ok1(err == 0);
		ok1(mode - (ssize_t)err <= 0 && mode + (ssize_t)err >= 0);
	}

	/* Works for big values too... */
	for (i = 0; i < 100; i++) {
		tally_add(tally, max - i);
		tally_add(tally, min + 1 + i);
		mode = tally_approx_mode(tally, &err);
		ok1(mode - (ssize_t)err <= 0 && mode + (ssize_t)err >= 0);
	}
	free(tally);

	tally = tally_new(10);
	tally_add(tally, 0);
	for (i = 0; i < 100; i++) {
		tally_add(tally, i);
		mode = tally_approx_mode(tally, &err);
		if (i < 10)
			ok1(err == 0);
		ok1(mode - (ssize_t)err <= 0 && mode + (ssize_t)err >= 0);
	}

	free(tally);
	return exit_status();
}
