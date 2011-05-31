#include <ccan/time/time.h>
#include <ccan/time/time.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct timeval t1, t2, t3, zero = { 0, 0 };

	plan_tests(46);

	/* Test time_now */
	t1 = time_now();
	t2 = time_now();

	/* Test time_sub. */
	t3 = time_sub(t2, t1);
	ok1(t3.tv_sec > 0 || t3.tv_usec >= 0);
	t3 = time_sub(t2, t2);
	ok1(t3.tv_sec == 0 && t3.tv_usec == 0);
	t3 = time_sub(t1, t1);
	ok1(t3.tv_sec == 0 && t3.tv_usec == 0);

	/* Test time_eq */
	ok1(time_eq(t1, t1));
	ok1(time_eq(t2, t2));
	ok1(!time_eq(t1, t3));
	ok1(!time_eq(t2, t3));

	/* Make sure t2 > t1. */
	t3.tv_sec = 0;
	t3.tv_usec = 1;
	t2 = time_add(t2, t3);

	/* Test time_less and time_greater. */
	ok1(!time_eq(t1, t2));
	ok1(!time_greater(t1, t2));
	ok1(time_less(t1, t2));
	ok1(time_greater(t2, t1));
	ok1(!time_less(t2, t1));
	t3.tv_sec = 0;
	t3.tv_usec = 999999;
	t2 = time_add(t2, t3);
	ok1(!time_eq(t1, t2));
	ok1(!time_greater(t1, t2));
	ok1(time_less(t1, t2));
	ok1(time_greater(t2, t1));
	ok1(!time_less(t2, t1));

	t3 = time_sub(t2, zero);
	ok1(time_eq(t3, t2));
	t3 = time_sub(t2, t2);
	ok1(time_eq(t3, zero));

	/* time_from_msec / time_to_msec */
	t3 = time_from_msec(500);
	ok1(t3.tv_sec == 0);
	ok1(t3.tv_usec == 500000);
	ok1(time_to_msec(t3) == 500);

	t3 = time_from_msec(1000);
	ok1(t3.tv_sec == 1);
	ok1(t3.tv_usec == 0);
	ok1(time_to_msec(t3) == 1000);

	t3 = time_from_msec(1500);
	ok1(t3.tv_sec == 1);
	ok1(t3.tv_usec == 500000);
	ok1(time_to_msec(t3) == 1500);

	/* time_from_usec */
	t3 = time_from_usec(500000);
	ok1(t3.tv_sec == 0);
	ok1(t3.tv_usec == 500000);
	ok1(time_to_usec(t3) == 500000);

	t3 = time_from_usec(1000000);
	ok1(t3.tv_sec == 1);
	ok1(t3.tv_usec == 0);
	ok1(time_to_usec(t3) == 1000000);

	t3 = time_from_usec(1500000);
	ok1(t3.tv_sec == 1);
	ok1(t3.tv_usec == 500000);
	ok1(time_to_usec(t3) == 1500000);

	/* Test wrapunder */
	t3 = time_sub(time_sub(t2, time_from_msec(500)), time_from_msec(500));
	ok1(t3.tv_sec == t2.tv_sec - 1);
	ok1(t3.tv_usec == t2.tv_usec);

	/* time_divide and time_multiply */
	t1.tv_usec = 100;
	t1.tv_sec = 100;

	t3 = time_divide(t1, 2);
	ok1(t3.tv_sec == 50);
	ok1(t3.tv_usec == 50);

	t3 = time_divide(t1, 100);
	ok1(t3.tv_sec == 1);
	ok1(t3.tv_usec == 1);

	t3 = time_multiply(t3, 100);
	ok1(time_eq(t3, t1));

	t3 = time_divide(t1, 200);
	ok1(t3.tv_sec == 0);
	ok1(t3.tv_usec == 500000);

	return exit_status();
}
