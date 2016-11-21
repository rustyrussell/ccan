/* Simple speed tests for jmap. */
#include <ccan/jmap/jmap.c>
#include <ccan/time/time.c>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

struct object {
	/* Some contents. Doubles as consistency check. */
	struct object *self;
};

struct jmap_obj {
	JMAP_MEMBERS(unsigned int, struct object *);
};

/* Nanoseconds per operation */
static size_t normalize(const struct timeabs *start,
			const struct timeabs *stop,
			unsigned int num)
{
	return time_to_nsec(time_divide(time_between(*stop, *start), num));
}

int main(int argc, char *argv[])
{
	struct object *objs;
	unsigned int i, j;
	size_t num;
	struct timeabs start, stop;
	struct jmap_obj *jmap;

	num = argv[1] ? atoi(argv[1]) : 1000000;
	objs = calloc(num, sizeof(objs[0]));

	for (i = 0; i < num; i++) {
		objs[i].self = &objs[i];
	}

	jmap = jmap_new(struct jmap_obj);

	printf("Initial insert: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		jmap_add(jmap, i, objs[i].self);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial lookup (match): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (jmap_get(jmap, i)->self != objs[i].self)
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial lookup (miss): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (jmap_get(jmap, i+num))
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	/* Lookups in order are very cache-friendly for judy; try random */
	printf("Initial lookup (random): ");
	fflush(stdout);
	start = time_now();
	for (i = 0, j = 0; i < num; i++, j = (j + 10007) % num)
		if (jmap_get(jmap, j)->self != &objs[j])
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial delete all: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		jmap_del(jmap, i);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial re-inserting: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		jmap_add(jmap, i, objs[i].self);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Deleting first half: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i+=2)
		jmap_del(jmap, i);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Adding (a different) half: ");
	fflush(stdout);

	start = time_now();
	for (i = 0; i < num; i+=2)
		jmap_add(jmap, num+i, objs[i].self);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after half-change (match): ");
	fflush(stdout);
	start = time_now();
	for (i = 1; i < num; i+=2)
		if (jmap_get(jmap, i)->self != objs[i].self)
			abort();
	for (i = 0; i < num; i+=2)
		if (jmap_get(jmap, i+num)->self != objs[i].self)
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after half-change(miss): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (jmap_get(jmap, i+num*2))
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	/* Hashtables with delete markers can fill with markers over time.
	 * so do some changes to see how it operates in long-term. */
	printf("Details: churning first time\n");
	for (i = 1; i < num; i+=2) {
		if (!jmap_del(jmap, i))
			abort();
		jmap_add(jmap, i, objs[i].self);
	}
	for (i = 0; i < num; i+=2) {
		if (!jmap_del(jmap, i+num))
			abort();
		jmap_add(jmap, i, objs[i].self);
	}
	for (i = 1; i < 5; i++) {
		printf("Churning %s time: ",
		       i == 1 ? "second"
		       : i == 2 ? "third"
		       : i == 3 ? "fourth"
		       : "fifth");
		fflush(stdout);

		start = time_now();
		for (j = 0; j < num; j++) {
			if (!jmap_del(jmap, num*(i-1)+j))
				abort();
			jmap_add(jmap, num*i+j, &objs[j]);
		}
		stop = time_now();
		printf(" %zu ns\n", normalize(&start, &stop, num));
	}

	/* Spread out the keys more to try to make it harder. */
	printf("Details: reinserting with spread\n");
	for (i = 0; i < num; i++) {
		if (!jmap_del(jmap, num*4 + i))
			abort();
		jmap_add(jmap, num * 5 + i * 9, objs[i].self);
	}

	if (jmap_popcount(jmap, 0, -1) != num)
		abort();

	printf("Lookup after churn & spread (match): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (jmap_get(jmap, num * 5 + i * 9)->self != objs[i].self) {
			printf("i  =%u\n", i);
			abort();
		}
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (miss): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (jmap_get(jmap, num * 6 + i * 9))
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (random): ");
	fflush(stdout);
	start = time_now();
	for (i = 0, j = 0; i < num; i++, j = (j + 10007) % num)
		if (jmap_get(jmap, num * 5 + j * 9)->self != &objs[j])
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (half-random): ");
	fflush(stdout);
	start = time_now();
	for (i = 0, j = 0; i < num/2; i++, j = (j + 10007) % num) {
		if (jmap_get(jmap, num * 5 + j * 9)->self != &objs[j])
			abort();
		if (jmap_get(jmap, num * 5 + (j + 1) * 9)->self != &objs[j+1])
			abort();
	}
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Deleting half after churn & spread: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i+=2)
		jmap_del(jmap, num * 5 + i * 9);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Adding (a different) half after churn & spread: ");
	fflush(stdout);

	start = time_now();
	for (i = 0; i < num; i+=2)
		jmap_add(jmap, num * 6 + i * 9, objs[i].self);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	jmap_free(jmap);
	free (objs);

	return 0;
}
