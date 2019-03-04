#define CCAN_TIMER_DEBUG
/* Include the C files directly. */
#include <ccan/timer/timer.c>
#include <ccan/tap/tap.h>

static void *test_alloc(size_t len, void *arg)
{
	(*(size_t *)arg)++;
	return malloc(len);
}

static void test_free(const void *p, void *arg)
{
	if (p) {
		(*(size_t *)arg)--;
		free((void *)p);
	}
}

static struct timemono timemono_from_nsec(unsigned long long nsec)
{
	struct timemono epoch = { { 0, 0 } };
	return timemono_add(epoch, time_from_nsec(nsec));
}

int main(void)
{
	struct timers timers;
	struct timer t[64];
	size_t num_allocs = 0;
	const struct timemono epoch = { { 0, 0 } };

	plan_tests(7);

	timers_set_allocator(test_alloc, test_free, &num_allocs);
	timers_init(&timers, epoch);
	timer_init(&t[0]);

	timer_addmono(&timers, &t[0],
		      timemono_from_nsec(TIMER_GRANULARITY << TIMER_LEVEL_BITS));
	timers_expire(&timers, timemono_from_nsec(1));
	ok1(num_allocs == 1);
	timer_del(&timers, &t[0]);
	ok1(num_allocs == 1);
	timers_cleanup(&timers);
	ok1(num_allocs == 0);

	/* Should restore defaults */
	timers_set_allocator(NULL, NULL, NULL);
	ok1(timer_alloc == timer_default_alloc);
	ok1(timer_free == timer_default_free);

	timers_init(&timers, epoch);
	timer_addmono(&timers, &t[0],
		      timemono_from_nsec(TIMER_GRANULARITY << TIMER_LEVEL_BITS));
	ok1(num_allocs == 0);
	timers_cleanup(&timers);
	ok1(num_allocs == 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
