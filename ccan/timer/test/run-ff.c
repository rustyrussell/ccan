#include <ccan/timer/timer.h>
/* Include the C files directly. */
#include <ccan/timer/timer.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct timers timers;
	struct timer t;
	struct list_head expired;

	/* This is how many tests you plan to run */
	plan_tests(3);

	timers_init(&timers, time_from_usec(1364726722653919ULL));
	timer_add(&timers, &t, time_from_usec(1364726722703919ULL));
	timers_expire(&timers, time_from_usec(1364726722653920ULL), &expired);
	ok1(list_empty(&expired));
	timers_expire(&timers, time_from_usec(1364726725454187ULL), &expired);
	ok1(!list_empty(&expired));
	ok1(list_top(&expired, struct timer, list) == &t);

	timers_cleanup(&timers);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
