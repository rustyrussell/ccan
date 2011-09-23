#include <ccan/tap/tap.h>
#include <ccan/jmap/jmap.c>

struct foo;

struct jmap_foo {
	JMAP_MEMBERS(unsigned long, struct foo *);
};

#define NUM 100

int main(int argc, char *argv[])
{
	struct jmap_foo *map;
	struct foo *foo[NUM], **foop;
	unsigned long i;

	plan_tests(40 + NUM*2 + 19);
	for (i = 0; i < NUM; i++)
		foo[i] = malloc(20);

	map = jmap_new(struct jmap_foo);
	ok1(jmap_error(map) == NULL);

	ok1(jmap_test(map, 0) == false);
	ok1(jmap_get(map, 0) == (struct foo *)NULL);
	ok1(jmap_popcount(map, 0, -1) == 0);
	ok1(jmap_first(map) == 0);
	ok1(jmap_last(map) == 0);
	ok1(jmap_del(map, 0) == false);

	/* Set only works on existing cases. */
	ok1(jmap_set(map, 1, foo[0]) == false);
	ok1(jmap_add(map, 1, foo[1]) == true);
	ok1(jmap_get(map, 1) == foo[1]);
	ok1(jmap_set(map, 1, foo[0]) == true);
	ok1(jmap_get(map, 1) == foo[0]);

	ok1(jmap_test(map, 1) == true);
	ok1(jmap_popcount(map, 0, -1) == 1);
	ok1(jmap_first(map) == 1);
	ok1(jmap_last(map) == 1);
	ok1(jmap_next(map, 0) == 1);
	ok1(jmap_next(map, 1) == 0);
	ok1(jmap_prev(map, 2) == 1);
	ok1(jmap_prev(map, 1) == 0);

	ok1(jmap_del(map, 1) == true);
	ok1(jmap_test(map, 1) == false);
	ok1(jmap_popcount(map, 0, -1) == 0);

	for (i = 0; i < NUM; i++)
		jmap_add(map, i+1, foo[i]);

	ok1(jmap_count(map) == NUM);
	ok1(jmap_popcount(map, 0, -1) == NUM);
	ok1(jmap_popcount(map, 1, NUM) == NUM);
	ok1(jmap_popcount(map, 1, NUM/2) == NUM/2);
	ok1(jmap_popcount(map, NUM/2+1, NUM) == NUM - NUM/2);

	ok1(jmap_nth(map, 0, -1) == 1);
	ok1(jmap_nth(map, NUM-1, -1) == NUM);
	ok1(jmap_nth(map, NUM, -1) == (size_t)-1);
	ok1(jmap_first(map) == 1);
	ok1(jmap_last(map) == NUM);
	ok1(jmap_next(map, 1) == 2);
	ok1(jmap_next(map, NUM) == 0);
	ok1(jmap_prev(map, 2) == 1);
	ok1(jmap_prev(map, 1) == 0);

	ok1(jmap_get(map, 1) == foo[0]);
	ok1(jmap_get(map, NUM) == foo[NUM-1]);
	ok1(jmap_get(map, NUM+1) == NULL);

	/* Reverse values in map. */
	for (i = 0; i < NUM; i++) {
		foop = jmap_getval(map, i+1);
		ok1(*foop == foo[i]);
		*foop = foo[NUM-1-i];
		jmap_putval(map, &foop);
	}
	for (i = 0; i < NUM; i++)
		ok1(jmap_get(map, i+1) == foo[NUM-1-i]);

	foop = jmap_nthval(map, 0, &i);
	ok1(i == 1);
	ok1(*foop == foo[NUM-1]);
	jmap_putval(map, &foop);
	foop = jmap_nthval(map, NUM-1, &i);
	ok1(i == NUM);
	ok1(*foop == foo[0]);
	jmap_putval(map, &foop);

	foop = jmap_firstval(map, &i);
	ok1(i == 1);
	ok1(*foop == foo[NUM-1]);
	jmap_putval(map, &foop);

	foop = jmap_nextval(map, &i);
	ok1(i == 2);
	ok1(*foop == foo[NUM-2]);
	jmap_putval(map, &foop);

	foop = jmap_prevval(map, &i);
	ok1(i == 1);
	ok1(*foop == foo[NUM-1]);
	jmap_putval(map, &foop);

	foop = jmap_prevval(map, &i);
	ok1(foop == NULL);

	foop = jmap_lastval(map, &i);
	ok1(i == NUM);
	ok1(*foop == foo[0]);
	jmap_putval(map, &foop);

	foop = jmap_prevval(map, &i);
	ok1(i == NUM-1);
	ok1(*foop == foo[1]);
	jmap_putval(map, &foop);

	foop = jmap_nextval(map, &i);
	ok1(i == NUM);
	ok1(*foop == foo[0]);
	jmap_putval(map, &foop);

	foop = jmap_nextval(map, &i);
	ok1(foop == NULL);

	ok1(jmap_error(map) == NULL);
	jmap_free(map);

	for (i = 0; i < NUM; i++)
		free(foo[i]);

	return exit_status();
}
