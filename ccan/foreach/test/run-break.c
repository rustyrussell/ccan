#include <ccan/foreach/foreach.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>
#include <ccan/foreach/foreach.c>

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
	int i, j, sum;

	plan_tests(2);

	sum = 0;
	foreach_int(i, 0, 1, 2, 3, 4) {
		foreach_int(j, 0, 1, 2, 3, 4) {
			sum += i*j;
			if (j == i)
				break;
		}
	}
	ok1(sum == 65);
	ok1(count_iters() <= 2);

	return exit_status();
}
       
