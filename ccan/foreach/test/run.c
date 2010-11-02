#include <ccan/foreach/foreach.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>
#include <ccan/foreach/foreach.c>

int main(void)
{
	int i, expecti;
	const char *p, *expectp[] = { "hello", "there", "big", "big", "world" };

	plan_tests(11);

	expecti = 0;
	foreach_int(i, 0, 1, 2, 3, 4) {
		ok1(i == expecti);
		expecti++;
	}

	expecti = 0;
	foreach_ptr(p, "hello", "there", "big", "big", "world") {
		ok1(strcmp(expectp[expecti], p) == 0);
		expecti++;
	}
	ok1(p == NULL);

	return exit_status();
}
       
