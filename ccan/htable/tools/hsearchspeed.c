/* Simple speed tests for a hash of strings using hsearch */
#include <ccan/htable/htable_type.h>
#include <ccan/htable/htable.c>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/talloc/talloc.h>
#include <ccan/hash/hash.h>
#include <ccan/time/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <search.h>

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

int main(int argc, char *argv[])
{
	size_t i, j, num;
	struct timeval start, stop;
	char **w;
	ENTRY *words, *misswords;

	w = strsplit(NULL, grab_file(NULL,
				     argv[1] ? argv[1] : "/usr/share/dict/words",
				     NULL), "\n");
	num = talloc_array_length(w) - 1;
	printf("%zu words\n", num);

	hcreate(num+num/3);

	words = talloc_array(w, ENTRY, num);
	for (i = 0; i < num; i++) {
		words[i].key = w[i];
		words[i].data = words[i].key;
	}

	/* Append and prepend last char for miss testing. */
	misswords = talloc_array(w, ENTRY, num);
	for (i = 0; i < num; i++) {
		char lastc;
		if (strlen(w[i]))
			lastc = w[i][strlen(w[i])-1];
		else
			lastc = 'z';
		misswords[i].key = talloc_asprintf(misswords, "%c%s%c%c",
						   lastc, w[i],
						   lastc, lastc);
	}

	printf("#01: Initial insert: ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		hsearch(words[i], ENTER);
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("#02: Initial lookup (match): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++)
		if (hsearch(words[i], FIND)->data != words[i].data)
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	printf("#03: Initial lookup (miss): ");
	fflush(stdout);
	start = time_now();
	for (i = 0; i < num; i++) {
		if (hsearch(misswords[i], FIND))
			abort();
	}
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	/* Lookups in order are very cache-friendly for judy; try random */
	printf("#04: Initial lookup (random): ");
	fflush(stdout);
	start = time_now();
	for (i = 0, j = 0; i < num; i++, j = (j + 10007) % num)
		if (hsearch(words[i], FIND)->data != words[i].data)
			abort();
	stop = time_now();
	printf(" %zu ns\n", normalize(&start, &stop, num));

	return 0;
}
