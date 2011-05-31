#include "config.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>

static int fake_gettimeofday(struct timeval *tv, struct timezone *tz);
static int fake_getrusage(int who, struct rusage *usage);
#define gettimeofday fake_gettimeofday
#define getrusage fake_getrusage

#include <ccan/lbalance/lbalance.c>
#include <ccan/tap/tap.h>

static unsigned faketime_ms = 0;
static struct rusage total_usage;

static int fake_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	assert(tz == NULL);
	tv->tv_usec = (faketime_ms % 1000) * 1000;
	tv->tv_sec = faketime_ms / 1000;
	return 0;
}

static int fake_getrusage(int who, struct rusage *usage)
{
	assert(who == RUSAGE_CHILDREN);
	*usage = total_usage;
	return 0;
}

static void test_optimum(struct lbalance *lb, unsigned int optimum)
{
	unsigned int j, i, num_tasks = 0, usec, num_counted = 0;
	float average;
	struct lbalance_task *tasks[1000];

	for (j = 0; j < 1000; j++) {
		diag("lbalance_target is %u\n", lbalance_target(lb));
		/* We measure average once we try optimum once. */
		if (lbalance_target(lb) == optimum && num_counted == 0) {
			average = lbalance_target(lb);
			num_counted = 1;
		} else if (num_counted) {
			average += lbalance_target(lb);
			num_counted++;
		}

		/* Create tasks until we reach target. */
		for (i = 0; i < lbalance_target(lb); i++) {
			tasks[i] = lbalance_task_new(lb);
		}
		num_tasks = i;

		faketime_ms += 100;
		/* If we're under optimum, set utilization to 100% */
		if (num_tasks <= optimum) {
			usec = 100000;
		} else {
			usec = 100000 * optimum / num_tasks;
		}

		for (i = 0; i < num_tasks; i++) {
			total_usage.ru_utime.tv_usec += usec / 2;
			if (total_usage.ru_utime.tv_usec > 1000000) {
				total_usage.ru_utime.tv_usec -= 1000000;
				total_usage.ru_utime.tv_sec++;
			}
			total_usage.ru_stime.tv_usec += usec / 2;
			if (total_usage.ru_stime.tv_usec > 1000000) {
				total_usage.ru_stime.tv_usec -= 1000000;
				total_usage.ru_stime.tv_sec++;
			}
			lbalance_task_free(tasks[i], NULL);
		}
	}

	/* We should have stayed close to optimum. */
	ok1(num_counted && (int)(average / num_counted + 0.5) == optimum);
}

int main(void)
{
	struct lbalance *lb;

	plan_tests(4);
	lb = lbalance_new();

	test_optimum(lb, 1);
	test_optimum(lb, 2);
	test_optimum(lb, 4);
	test_optimum(lb, 64);
	lbalance_free(lb);

	return exit_status();
}
