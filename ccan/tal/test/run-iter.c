#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

#define NUM 1000

int main(void)
{
	char *p[NUM], *iter;
	int i;

	plan_tests(NUM + 1 + NUM);

	/* Create a random tree, but make sure we get multiple
	 * top-level groups! */
	for (i = 0; i < NUM; i++) {
		p[i] = tal(NULL, char);
		*p[i] = '0';
		if (next_group(&null_parent.c.group) != &null_parent.c.group)
			break;
	}
	for (i++; i < NUM; i++) {
		p[i] = tal(p[rand() % i], char);
		*p[i] = '0';
	}

	i = 0;
	for (iter = tal_first(NULL); iter; iter = tal_next(NULL, iter)) {
		i++;
		ok1(*iter == '0');
		*iter = '1';
	}
	ok1(i == NUM);

	for (i = NUM-1; i >= 0; i--) {
		ok1(*p[i] == '1');
		tal_free(p[i]);
	}
	return exit_status();
}
