#include <ccan/tap/tap.h>
#include <ccan/jmap/jmap.c>

struct foo;
struct idx;

struct jmap_foo {
	JMAP_MEMBERS(struct idx *, struct foo *);
};

#define NUM 100

static int cmp_ptr(const void *a, const void *b)
{
	return *(char **)a - *(char **)b;
}

int main(int argc, char *argv[])
{
	struct jmap_foo *map;
	struct foo *foo[NUM+1], **foop;
	struct idx *idx[NUM+1], *index;

	unsigned int i;

	plan_tests(25 + NUM*2 + 6);
	for (i = 0; i < NUM+1; i++)
		foo[i] = malloc(20);

	qsort(foo, NUM, sizeof(foo[0]), cmp_ptr);

	/* idx[i] == foo[i] + 1, for easy checking */
	for (i = 0; i < NUM+1; i++)
		idx[i] = (void *)((char *)foo[i] + 1);

	map = jmap_new(struct jmap_foo);
	ok1(jmap_error(map) == NULL);

	ok1(jmap_test(map, idx[NUM]) == false);
	ok1(jmap_get(map, idx[NUM]) == (struct foo *)NULL);
	ok1(jmap_count(map) == 0);
	ok1(jmap_first(map) == (struct idx *)NULL);
	ok1(jmap_del(map, idx[0]) == false);

	/* Set only works on existing cases. */
	ok1(jmap_set(map, idx[0], foo[0]) == false);
	ok1(jmap_add(map, idx[0], foo[1]) == true);
	ok1(jmap_get(map, idx[0]) == foo[1]);
	ok1(jmap_set(map, idx[0], foo[0]) == true);
	ok1(jmap_get(map, idx[0]) == foo[0]);

	ok1(jmap_test(map, idx[0]) == true);
	ok1(jmap_count(map) == 1);
	ok1(jmap_first(map) == idx[0]);
	ok1(jmap_next(map, idx[0]) == NULL);

	ok1(jmap_del(map, idx[0]) == true);
	ok1(jmap_test(map, idx[0]) == false);
	ok1(jmap_count(map) == 0);

	for (i = 0; i < NUM; i++)
		jmap_add(map, idx[i], foo[i]);

	ok1(jmap_count(map) == NUM);

	ok1(jmap_first(map) == idx[0]);
	ok1(jmap_next(map, idx[0]) == idx[1]);
	ok1(jmap_next(map, idx[NUM-1]) == NULL);

	ok1(jmap_get(map, idx[0]) == foo[0]);
	ok1(jmap_get(map, idx[NUM-1]) == foo[NUM-1]);
	ok1(jmap_get(map, (void *)((char *)idx[NUM-1] + 1)) == NULL);

	/* Reverse values in map. */
	for (i = 0; i < NUM; i++) {
		foop = jmap_getval(map, idx[i]);
		ok1(*foop == foo[i]);
		*foop = foo[NUM-1-i];
		jmap_putval(map, &foop);
	}
	for (i = 0; i < NUM; i++)
		ok1(jmap_get(map, idx[i]) == foo[NUM-1-i]);

	foop = jmap_firstval(map, &index);
	ok1(index == idx[0]);
	ok1(*foop == foo[NUM-1]);
	jmap_putval(map, &foop);

	foop = jmap_nextval(map, &index);
	ok1(index == idx[1]);
	ok1(*foop == foo[NUM-2]);
	jmap_putval(map, &foop);

	index = idx[NUM-1];
	foop = jmap_nextval(map, &index);
	ok1(foop == NULL);

	ok1(jmap_error(map) == NULL);
	jmap_free(map);

	for (i = 0; i < NUM+1; i++)
		free(foo[i]);

	return exit_status();
}
