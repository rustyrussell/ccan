#include <ccan/tap/tap.h>
#include <ccan/jmap/jmap_type.h>
#include <ccan/jmap/jmap.c>

struct foo;

JMAP_DEFINE_UINTIDX_TYPE(struct foo, foo);

#define NUM 100

int main(int argc, char *argv[])
{
	struct jmap_foo *map;
	struct foo *foo[NUM], **foop;
	unsigned long i;

	plan_tests(37 + NUM*2 + 19);
	for (i = 0; i < NUM; i++)
		foo[i] = malloc(20);

	map = jmap_foo_new();
	ok1(jmap_foo_error(map) == NULL);

	ok1(jmap_foo_test(map, 0) == false);
	ok1(jmap_foo_get(map, 0) == (struct foo *)NULL);
	ok1(jmap_foo_popcount(map, 0, -1) == 0);
	ok1(jmap_foo_first(map, 0) == 0);
	ok1(jmap_foo_last(map, 0) == 0);
	ok1(jmap_foo_del(map, 0) == false);

	/* Set only works on existing cases. */
	ok1(jmap_foo_set(map, 0, foo[0]) == false);
	ok1(jmap_foo_add(map, 0, foo[1]) == true);
	ok1(jmap_foo_get(map, 0) == foo[1]);
	ok1(jmap_foo_set(map, 0, foo[0]) == true);
	ok1(jmap_foo_get(map, 0) == foo[0]);

	ok1(jmap_foo_test(map, 0) == true);
	ok1(jmap_foo_popcount(map, 0, -1) == 1);
	ok1(jmap_foo_first(map, -1) == 0);
	ok1(jmap_foo_last(map, -1) == 0);
	ok1(jmap_foo_next(map, 0, -1) == (size_t)-1);
	ok1(jmap_foo_prev(map, 0, -1) == (size_t)-1);

	ok1(jmap_foo_del(map, 0) == true);
	ok1(jmap_foo_test(map, 0) == false);
	ok1(jmap_foo_popcount(map, 0, -1) == 0);

	for (i = 0; i < NUM; i++)
		jmap_foo_add(map, i, foo[i]);

	ok1(jmap_foo_popcount(map, 0, -1) == NUM);
	ok1(jmap_foo_popcount(map, 0, NUM-1) == NUM);
	ok1(jmap_foo_popcount(map, 0, NUM/2-1) == NUM/2);
	ok1(jmap_foo_popcount(map, NUM/2, NUM) == NUM - NUM/2);

	ok1(jmap_foo_nth(map, 0, -1) == 0);
	ok1(jmap_foo_nth(map, NUM-1, -1) == NUM-1);
	ok1(jmap_foo_nth(map, NUM, -1) == (size_t)-1);
	ok1(jmap_foo_first(map, -1) == 0);
	ok1(jmap_foo_last(map, -1) == NUM-1);
	ok1(jmap_foo_next(map, 0, -1) == 1);
	ok1(jmap_foo_next(map, NUM-1, -1) == (size_t)-1);
	ok1(jmap_foo_prev(map, 1, -1) == 0);
	ok1(jmap_foo_prev(map, 0, -1) == (size_t)-1);

	ok1(jmap_foo_get(map, 0) == foo[0]);
	ok1(jmap_foo_get(map, NUM-1) == foo[NUM-1]);
	ok1(jmap_foo_get(map, NUM) == NULL);

	/* Reverse values in map. */
	for (i = 0; i < NUM; i++) {
		foop = jmap_foo_getval(map, i);
		ok1(*foop == foo[i]);
		*foop = foo[NUM-1-i];
		jmap_foo_putval(map, &foop);
	}
	for (i = 0; i < NUM; i++)
		ok1(jmap_foo_get(map, i) == foo[NUM-1-i]);

	foop = jmap_foo_nthval(map, 0, &i);
	ok1(i == 0);
	ok1(*foop == foo[NUM-1]);
	jmap_foo_putval(map, &foop);
	foop = jmap_foo_nthval(map, NUM-1, &i);
	ok1(i == NUM-1);
	ok1(*foop == foo[0]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_firstval(map, &i);
	ok1(i == 0);
	ok1(*foop == foo[NUM-1]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_nextval(map, &i);
	ok1(i == 1);
	ok1(*foop == foo[NUM-2]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_prevval(map, &i);
	ok1(i == 0);
	ok1(*foop == foo[NUM-1]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_prevval(map, &i);
	ok1(foop == NULL);

	foop = jmap_foo_lastval(map, &i);
	ok1(i == NUM-1);
	ok1(*foop == foo[0]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_prevval(map, &i);
	ok1(i == NUM-2);
	ok1(*foop == foo[1]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_nextval(map, &i);
	ok1(i == NUM-1);
	ok1(*foop == foo[0]);
	jmap_foo_putval(map, &foop);

	foop = jmap_foo_nextval(map, &i);
	ok1(foop == NULL);

	ok1(jmap_foo_error(map) == NULL);
	jmap_foo_free(map);

	return exit_status();
}
