#include <ccan/tap/tap.h>
#include <ccan/jset/jset.c>

struct foo;

struct jset_foo {
	JSET_MEMBERS(struct foo *);
};

static int cmp_ptr(const void *a, const void *b)
{
	return *(char **)a - *(char **)b;
}

#define NUM 100

int main(int argc, char *argv[])
{
	struct jset_foo *set;
	struct foo *foo[NUM];
	unsigned int i;

	plan_tests(20);
	for (i = 0; i < NUM; i++)
		foo[i] = malloc(20);

	set = jset_new(struct jset_foo);
	ok1(jset_error(set) == NULL);

	ok1(jset_set(set, foo[0]) == true);
	ok1(jset_set(set, foo[0]) == false);
	ok1(jset_clear(set, foo[0]) == true);
	ok1(jset_clear(set, foo[0]) == false);
	ok1(jset_count(set) == 0);
	ok1(jset_nth(set, 0, NULL) == (struct foo *)NULL);
	ok1(jset_first(set) == (struct foo *)NULL);
	ok1(jset_last(set) == (struct foo *)NULL);

	for (i = 0; i < NUM; i++)
		jset_set(set, foo[i]);

	qsort(foo, NUM, sizeof(foo[0]), cmp_ptr);

	ok1(jset_count(set) == NUM);
	ok1(jset_nth(set, 0, NULL) == foo[0]);
	ok1(jset_nth(set, NUM-1, NULL) == foo[NUM-1]);
	ok1(jset_nth(set, NUM, NULL) == (struct foo *)NULL);
	ok1(jset_first(set) == foo[0]);
	ok1(jset_last(set) == foo[NUM-1]);
	ok1(jset_next(set, foo[0]) == foo[1]);
	ok1(jset_next(set, foo[NUM-1]) == (struct foo *)NULL);
	ok1(jset_prev(set, foo[1]) == foo[0]);
	ok1(jset_prev(set, foo[0]) == (struct foo *)NULL);
	ok1(jset_error(set) == NULL);
	jset_free(set);

	for (i = 0; i < NUM; i++)
		free(foo[i]);

	return exit_status();
}
