#include <ccan/foreach/foreach.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ccan/foreach/foreach.c>

static int test_int_recursion(unsigned int level)
{
	int i, sum = 0;

	foreach_int(i, 0, 1, 2, 3, 4) {
		sum += i;
		if (i > level)
			sum += test_int_recursion(i);
	}
	return sum;
}

static int test_ptr_recursion(const char *level)
{
	int sum = 0;
	const char *i;

	foreach_ptr(i, "0", "1", "2", "3", "4") {
		sum += atoi(i);
		if (atoi(i) > atoi(level))
			sum += test_ptr_recursion(i);
	}
	return sum;
}

static int count_iters(void)
{
	unsigned int count = 0;
#if !HAVE_COMPOUND_LITERALS || !HAVE_FOR_LOOP_DECLARATION
	struct iter_info *i;
	
	list_for_each(&iters, i, list)
		count++;
#endif
	return count;
}

int main(void)
{
	int i, j, sum, max_iters;
	const char *istr, *jstr;

	plan_tests(13);

	sum = 0;
	foreach_int(i, 0, 1, 2, 3, 4)
		foreach_int(j, 0, 1, 2, 3, 4)
			sum += i*j;
	diag("sum = %i\n", sum);
	diag("iters = %i\n", count_iters());
	ok1(sum == 100);
	ok1(count_iters() <= 2);

	/* Same again... reusing iterators. */
	sum = 0;
	foreach_int(i, 0, 1, 2, 3, 4)
		foreach_int(j, 0, 1, 2, 3, 4)
			sum += i*j;
	diag("sum = %i\n", sum);
	diag("iters = %i\n", count_iters());
	ok1(sum == 100);
	ok1(count_iters() <= 2);

	sum = 0;
	foreach_ptr(istr, "0", "1", "2", "3", "4")
		foreach_ptr(jstr, "0", "1", "2", "3", "4")
			sum += atoi(istr)*atoi(jstr);
	diag("sum = %i\n", sum);
	diag("iters = %i\n", count_iters());
	ok1(sum == 100);
	ok1(count_iters() <= 2 + 2);

	/* Same again... reusing iterators. */
	sum = 0;
	foreach_ptr(istr, "0", "1", "2", "3", "4")
		foreach_ptr(jstr, "0", "1", "2", "3", "4")
			sum += atoi(istr)*atoi(jstr);
	diag("sum = %i\n", sum);
	diag("iters = %i\n", count_iters());
	ok1(sum == 100);
	ok1(count_iters() <= 2 + 2);

	/* Do this twice, second time shouldn't increase iterators. */
	for (i = 0; i < 2; i++) {
		sum = test_int_recursion(0);
		diag("sum = %i\n", sum);
		diag("iters = %i\n", count_iters());
		ok1(sum == 160);

		sum = test_ptr_recursion("0");
		diag("sum = %i\n", sum);
		diag("iters = %i\n", count_iters());
		ok1(sum == 160);
		if (i == 0)
			max_iters = count_iters();
		else
			ok1(count_iters() <= max_iters);
	}
	return exit_status();
}
       
