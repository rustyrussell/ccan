#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct tally *tally;
	ssize_t total, overflow;
	ssize_t min, max;

	max = (ssize_t)~(1ULL << (sizeof(max)*CHAR_BIT - 1));
	min = (ssize_t)(1ULL << (sizeof(max)*CHAR_BIT - 1));

	plan_tests(15);

	/* Simple case. */
	tally = tally_new(0);
	tally_add(tally, min);
	ok1(tally_total(tally, NULL) == min);
	ok1(tally_total(tally, &overflow) == min);
	ok1(overflow == -1);

	/* Underflow. */
	tally_add(tally, min);
	total = tally_total(tally, &overflow);
	ok1(overflow == -1);
	ok1((size_t)total == 0);
	ok1(tally_total(tally, NULL) == min);
	free(tally);

	/* Simple case. */
	tally = tally_new(0);
	tally_add(tally, max);
	ok1(tally_total(tally, NULL) == max);
	ok1(tally_total(tally, &overflow) == max);
	ok1(overflow == 0);

	/* Overflow into sign bit... */
	tally_add(tally, max);
	total = tally_total(tally, &overflow);
	ok1(overflow == 0);
	ok1((size_t)total == (size_t)-2);
	ok1(tally_total(tally, NULL) == max);

	/* Overflow into upper size_t. */
	tally_add(tally, max);
	total = tally_total(tally, &overflow);
	ok1(overflow == 1);
	if (sizeof(size_t) == 4)
		ok1((size_t)total == 0x7FFFFFFD);
	else if (sizeof(size_t) == 8)
		ok1((size_t)total == 0x7FFFFFFFFFFFFFFDULL);
	ok1(tally_total(tally, NULL) == max);
	free(tally);

	return exit_status();
}
