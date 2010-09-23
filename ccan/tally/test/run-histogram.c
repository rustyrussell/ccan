#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	int i;
	struct tally *tally;
	char *graph, *p;
	bool trunc;

	plan_tests(100 + 1 + 10 + 1 + 100 + 1 + 10 + 1 + 10 + 2 + 1);

	/* Uniform distribution, easy. */
	tally = tally_new(100);
	for (i = 0; i < 100; i++)
		tally_add(tally, i);

	/* 1:1 height. */
	graph = p = tally_histogram(tally, 20, 100);
	for (i = 0; i < 100; i++) {
		char *eol = strchr(p, '\n');

		/* We expect it filled all way to the end. */
		ok1(eol - p == 20);
		p = eol + 1;
	}
	ok1(!*p);
	free(graph);

	/* Reduced height. */
	trunc = false;
	graph = p = tally_histogram(tally, 20, 10);
	for (i = 0; i < 10; i++) {
		char *eol = strchr(p, '\n');

		/* Last once can be truncated (bucket aliasing) */
		if (eol) {
			if (eol - p < 20) {
				ok1(!trunc);
				trunc = true;
			} else if (eol - p == 20) {
				ok1(!trunc);
			} else {
				fail("Overwidth line %s", p);
			}
		} else
			/* We should, at worst, half-fill graph */
			ok1(i > 5);

		if (eol)
			p = eol + 1;
	}
	ok1(!*p);
	free(graph);

	/* Enlarged height (gets capped). */
	graph = p = tally_histogram(tally, 20, 1000);
	for (i = 0; i < 100; i++) {
		char *eol = strchr(p, '\n');
		/* We expect it filled all way to the end. */
		ok1(eol - p == 20);
		p = eol + 1;
	}
	ok1(!*p);
	free(graph);
	free(tally);

	/* Distinctive increasing pattern. */
	tally = tally_new(10);
	for (i = 0; i < 10; i++) {
		unsigned int j;
		for (j = 0; j <= i; j++)
			tally_add(tally, i);
	}

	graph = p = tally_histogram(tally, 10, 10);
	for (i = 0; i < 10; i++) {
		char *eol = strchr(p, '\n');
		ok1(eol - p == i+1);
		p = eol + 1;
	}
	ok1(!*p);
	diag("Here's the pretty: %s", graph);
	free(graph);
	free(tally);

	/* With negative values. */
	tally = tally_new(10);
	for (i = 0; i < 10; i++) {
		tally_add(tally, i - 5);
	}

	graph = p = tally_histogram(tally, 10, 10);
	for (i = 0; i < 10; i++) {
		char *eol = strchr(p, '\n');

		/* We expect it filled all way to the end. */
		ok1(eol - p == 10);

		/* Check min/max labels. */
		if (i == 0)
			ok1(strncmp(p, "-5*", 3) == 0);
		if (i == 9)
			ok1(strncmp(p, "4*", 2) == 0);
		p = eol + 1;
	}
	ok1(!*p);
	free(graph);
	free(tally);

	return exit_status();
}
