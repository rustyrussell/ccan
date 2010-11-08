/* Simple speed tests for jmap. */
#include <ccan/jmap/jmap_type.h>
#include <ccan/jmap/jmap.c>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

struct object {
	/* Some contents. Doubles as consistency check. */
	struct object *self;
};

/* Nanoseconds per operation */
static size_t normalize(const struct timeval *start,
			const struct timeval *stop,
			unsigned int num)
{
	struct timeval diff;

	timersub(stop, start, &diff);

	/* Floating point is more accurate here. */
	return (double)(diff.tv_sec * 1000000 + diff.tv_usec)
		/ num * 1000;
}

JMAP_DEFINE_UINTIDX_TYPE(struct object, obj);

int main(int argc, char *argv[])
{
	struct object *objs;
	size_t i, j, num;
	struct timeval start, stop;
	struct jmap_obj *jmap;

	num = argv[1] ? atoi(argv[1]) : 1000000;
	objs = calloc(num, sizeof(objs[0]));

	for (i = 0; i < num; i++) {
		objs[i].self = &objs[i];
	}

	jmap = jmap_obj_new();

	printf("Initial insert: ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		jmap_obj_add(jmap, i, objs[i].self);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial lookup (match): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (jmap_obj_get(jmap, i)->self != objs[i].self)
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial lookup (miss): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (jmap_obj_get(jmap, i+num))
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	/* Lookups in order are very cache-friendly for judy; try random */
	printf("Initial lookup (random): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0, j = 0; i < num; i++, j = (j + 10007) % num)
		if (jmap_obj_get(jmap, j)->self != &objs[j])
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial delete all: ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		jmap_obj_del(jmap, i);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Initial re-inserting: ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		jmap_obj_add(jmap, i, objs[i].self);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Deleting first half: ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i+=2)
		jmap_obj_del(jmap, i);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Adding (a different) half: ");
	fflush(stdout);

	gettimeofday(&start, NULL);
	for (i = 0; i < num; i+=2)
		jmap_obj_add(jmap, num+i, objs[i].self);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after half-change (match): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 1; i < num; i+=2)
		if (jmap_obj_get(jmap, i)->self != objs[i].self)
			abort();
	for (i = 0; i < num; i+=2)
		if (jmap_obj_get(jmap, i+num)->self != objs[i].self)
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after half-change(miss): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (jmap_obj_get(jmap, i+num*2))
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	/* Hashtables with delete markers can fill with markers over time.
	 * so do some changes to see how it operates in long-term. */
	printf("Details: churning first time\n");
	for (i = 1; i < num; i+=2) {
		if (!jmap_obj_del(jmap, i))
			abort();
		jmap_obj_add(jmap, i, objs[i].self);
	}
	for (i = 0; i < num; i+=2) {
		if (!jmap_obj_del(jmap, i+num))
			abort();
		jmap_obj_add(jmap, i, objs[i].self);
	}
	for (i = 1; i < 5; i++) {
		printf("Churning %s time: ",
		       i == 1 ? "second"
		       : i == 2 ? "third"
		       : i == 3 ? "fourth"
		       : "fifth");
		fflush(stdout);
		gettimeofday(&start, NULL);
		for (j = 0; j < num; j++) {
			if (!jmap_obj_del(jmap, num*(i-1)+j))
				abort();
			jmap_obj_add(jmap, num*i+j, &objs[j]);
		}
		gettimeofday(&stop, NULL);
		printf(" %zu ns\n", normalize(&start, &stop, num));
	}

	/* Spread out the keys more to try to make it harder. */
	printf("Details: reinserting with spread\n");
	for (i = 0; i < num; i++) {
		if (!jmap_obj_del(jmap, num*4 + i))
			abort();
		jmap_obj_add(jmap, num * 5 + i * 9, objs[i].self);
	}

	if (jmap_obj_popcount(jmap, 0, -1) != num)
		abort();

	printf("Lookup after churn & spread (match): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (jmap_obj_get(jmap, num * 5 + i * 9)->self != objs[i].self) {
			printf("i  =%u\n", i);
			abort();
		}
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (miss): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i++)
		if (jmap_obj_get(jmap, num * 6 + i * 9))
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (random): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0, j = 0; i < num; i++, j = (j + 10007) % num)
		if (jmap_obj_get(jmap, num * 5 + j * 9)->self != &objs[j])
			abort();
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Lookup after churn & spread (half-random): ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0, j = 0; i < num/2; i++, j = (j + 10007) % num) {
		if (jmap_obj_get(jmap, num * 5 + j * 9)->self != &objs[j])
			abort();
		if (jmap_obj_get(jmap, num * 5 + (j + 1) * 9)->self != &objs[j+1])
			abort();
	}
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Deleting half after churn & spread: ");
	fflush(stdout);
	gettimeofday(&start, NULL);
	for (i = 0; i < num; i+=2)
		jmap_obj_del(jmap, num * 5 + i * 9);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("Adding (a different) half after churn & spread: ");
	fflush(stdout);

	gettimeofday(&start, NULL);
	for (i = 0; i < num; i+=2)
		jmap_obj_add(jmap, num * 6 + i * 9, objs[i].self);
	gettimeofday(&stop, NULL);
	printf(" %zu ns\n", normalize(&start, &stop, num));

	return 0;
}
