#include <ccan/timer/timer.h>
/* Include the C files directly. */
#include <ccan/timer/timer.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct timers timers;
	struct timer t;
	struct list_head list;

	/* This is how many tests you plan to run */
	plan_tests(7);

	timers_init(&timers, grains_to_time(1364984760903400ULL));
	ok1(timers.base == 1364984760903400ULL);
	timer_add(&timers, &t, grains_to_time(1364984761003398ULL));
	ok1(t.time == 1364984761003398ULL);
	ok1(timers.first == 1364984761003398ULL);
	timers_expire(&timers, grains_to_time(1364984760903444ULL), &list);
	ok1(timers_check(&timers, NULL));
	ok1(list_pop(&list, struct timer, list) == NULL);
	timers_expire(&timers, grains_to_time(1364984761002667ULL), &list);
	ok1(timers_check(&timers, NULL));
	ok1(list_pop(&list, struct timer, list) == NULL);

	timers_cleanup(&timers);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
