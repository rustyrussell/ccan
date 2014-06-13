#include <ccan/time/time.h>
#include <ccan/time/time.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct timemono t1, t2;
	struct timelen t3;

	plan_tests(3);

	/* Test time_mono */
	t1 = time_mono();
	t2 = time_mono();

	ok1(!time_is_before(t2, t1));
	t2.ts = time_add_(t1.ts, time_from_nsec(1).ts);
	ok1(time_is_after(t2, t1));

	t3.ts.tv_sec = 1;
	t3.ts.tv_nsec = 0;

	ok1(time_less(timemono_between(t1, t2), t3));

	return exit_status();
}
