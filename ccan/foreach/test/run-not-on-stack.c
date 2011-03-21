#include <ccan/foreach/foreach.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ccan/foreach/foreach.c>

static int global_i;
static void *allocs[1000];
static unsigned int num_allocs;

static void iterate(unsigned int depth, bool done_global)
{
	int *i, expecti;
	const char *expectp[] = { "hello", "there" };
	const char **p;
	int stack_i;

	if (depth == 4)
		return;

	if (!done_global) {
		expecti = 0;
		foreach_int(global_i, 0, 1) {
			ok1(global_i == expecti);
			expecti++;
			if (global_i == 0)
				iterate(depth + 1, true);
		}
		ok1(expecti == 2);
	}

	i = allocs[num_allocs++] = malloc(sizeof(*i));
	expecti = 0;
	foreach_int(*i, 0, 1) {
		ok1(*i == expecti);
		expecti++;
		if (*i == 0)
			iterate(depth + 1, done_global);
	}
	ok1(expecti == 2);

	p = allocs[num_allocs++] = malloc(sizeof(*p));
	expecti = 0;
	foreach_ptr(*p, "hello", "there") {
		ok1(strcmp(expectp[expecti], *p) == 0);
		expecti++;
		if (expecti == 1)
			iterate(depth + 1, done_global);
	}
	ok1(expecti == 2);
	ok1(*p == NULL);

	expecti = 0;
	foreach_int(stack_i, 0, 1) {
		ok1(stack_i == expecti);
		expecti++;
		if (stack_i == 0)
			iterate(depth + 1, done_global);
	}
	ok1(expecti == 2);
}

int main(void)
{
	unsigned int i;
	plan_tests(861);

	iterate(0, false);

	ok1(num_allocs < 1000);
	for (i = 0; i < num_allocs; i++)
		free(allocs[i]);

	return exit_status();
}
