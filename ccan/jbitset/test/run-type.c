#include <ccan/tap/tap.h>
#include <ccan/jbitset/jbitset_type.h>
#include <ccan/jbitset/jbitset.c>

struct foo;

JBIT_DEFINE_TYPE(struct foo, foo);

static int cmp_ptr(const void *a, const void *b)
{
	return *(char **)a - *(char **)b;
}

#define NUM 100

int main(int argc, char *argv[])
{
	struct jbitset_foo *set;
	struct foo *foo[NUM];
	unsigned int i;

	plan_tests(20);
	for (i = 0; i < NUM; i++)
		foo[i] = malloc(20);

	set = jbit_foo_new();
	ok1(jbit_foo_error(set) == NULL);

	ok1(jbit_foo_set(set, foo[0]) == true);
	ok1(jbit_foo_set(set, foo[0]) == false);
	ok1(jbit_foo_clear(set, foo[0]) == true);
	ok1(jbit_foo_clear(set, foo[0]) == false);
	ok1(jbit_foo_count(set) == 0);
	ok1(jbit_foo_nth(set, 0) == (struct foo *)NULL);
	ok1(jbit_foo_first(set) == (struct foo *)NULL);
	ok1(jbit_foo_last(set) == (struct foo *)NULL);

	for (i = 0; i < NUM; i++)
		jbit_foo_set(set, foo[i]);

	qsort(foo, NUM, sizeof(foo[0]), cmp_ptr);

	ok1(jbit_foo_count(set) == NUM);
	ok1(jbit_foo_nth(set, 0) == foo[0]);
	ok1(jbit_foo_nth(set, NUM-1) == foo[NUM-1]);
	ok1(jbit_foo_nth(set, NUM) == (struct foo *)NULL);
	ok1(jbit_foo_first(set) == foo[0]);
	ok1(jbit_foo_last(set) == foo[NUM-1]);
	ok1(jbit_foo_next(set, foo[0]) == foo[1]);
	ok1(jbit_foo_next(set, foo[NUM-1]) == (struct foo *)NULL);
	ok1(jbit_foo_prev(set, foo[1]) == foo[0]);
	ok1(jbit_foo_prev(set, foo[0]) == (struct foo *)NULL);
	ok1(jbit_foo_error(set) == NULL);
	jbit_foo_free(set);

	for (i = 0; i < NUM; i++)
		free(foo[i]);

	return exit_status();
}
