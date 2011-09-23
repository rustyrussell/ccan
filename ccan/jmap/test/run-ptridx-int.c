#include <ccan/tap/tap.h>
#include <ccan/jmap/jmap.c>

struct idx;

struct map {
	JMAP_MEMBERS(struct idx *, int);
};

#define NUM 100

static int cmp_ptr(const void *a, const void *b)
{
	return *(char **)a - *(char **)b;
}

int main(int argc, char *argv[])
{
	struct map *map;
	struct idx *idx[NUM+1], *index;
	unsigned int i;
	int *intp;

	plan_tests(25 + NUM*2 + 6);
	for (i = 0; i < NUM+1; i++)
		idx[i] = malloc(20);

	qsort(idx, NUM, sizeof(idx[0]), cmp_ptr);

	map = jmap_new(struct map);
	ok1(jmap_error(map) == NULL);

	ok1(jmap_test(map, idx[NUM]) == false);
	ok1(jmap_get(map, idx[NUM]) == 0);
	ok1(jmap_count(map) == 0);
	ok1(jmap_first(map) == 0);
	ok1(jmap_del(map, idx[0]) == false);

	/* Set only works on existing cases. */
	ok1(jmap_set(map, idx[0], 0) == false);
	ok1(jmap_add(map, idx[0], 1) == true);
	ok1(jmap_get(map, idx[0]) == 1);
	ok1(jmap_set(map, idx[0], -1) == true);
	ok1(jmap_get(map, idx[0]) == -1);

	ok1(jmap_test(map, idx[0]) == true);
	ok1(jmap_count(map) == 1);
	ok1(jmap_first(map) == idx[0]);
	ok1(jmap_next(map, idx[0]) == NULL);

	ok1(jmap_del(map, idx[0]) == true);
	ok1(jmap_test(map, idx[0]) == false);
	ok1(jmap_count(map) == 0);

	for (i = 0; i < NUM; i++)
		jmap_add(map, idx[i], i+1);

	ok1(jmap_count(map) == NUM);

	ok1(jmap_first(map) == idx[0]);
	ok1(jmap_next(map, idx[0]) == idx[1]);
	ok1(jmap_next(map, idx[NUM-1]) == NULL);

	ok1(jmap_get(map, idx[0]) == 1);
	ok1(jmap_get(map, idx[NUM-1]) == NUM);
	ok1(jmap_get(map, (void *)((char *)idx[NUM-1] + 1)) == 0);

	/* Reverse values in map. */
	for (i = 0; i < NUM; i++) {
		intp = jmap_getval(map, idx[i]);
		ok1(*intp == i+1);
		*intp = NUM-i;
		jmap_putval(map, &intp);
	}
	for (i = 0; i < NUM; i++)
		ok1(jmap_get(map, idx[i]) == NUM-i);

	intp = jmap_firstval(map, &index);
	ok1(index == idx[0]);
	ok1(*intp == NUM);
	jmap_putval(map, &intp);

	intp = jmap_nextval(map, &index);
	ok1(index == idx[1]);
	ok1(*intp == NUM-1);
	jmap_putval(map, &intp);

	index = idx[NUM-1];
	intp = jmap_nextval(map, &index);
	ok1(intp == NULL);

	ok1(jmap_error(map) == NULL);
	jmap_free(map);

	for (i = 0; i < NUM+1; i++)
		free(idx[i]);

	return exit_status();
}
