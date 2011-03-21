#include <ccan/foreach/foreach.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>
#include <ccan/foreach/foreach.c>

int main(void)
{
	int i, num;
	const char *p;

	plan_tests(5);

	num = 0;
	foreach_int(i, 0) {
		ok1(i == 0);
		num++;
	}
	ok1(num == 1);

	num = 0;
	foreach_ptr(p, "hello") {
		ok1(strcmp("hello", p) == 0);
		num++;
	}
	ok1(p == NULL);
	ok1(num == 1);

	return exit_status();
}
       
