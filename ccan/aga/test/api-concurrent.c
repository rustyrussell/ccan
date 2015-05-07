#include "config.h"

#include <stddef.h>
#include <assert.h>

#include <ccan/aga/aga.h>

#include <ccan/tap/tap.h>

#include "simple-graph.h"


#define NUM_ALGOS	2

#define check_one_inner(algo) \
	ok1(aga_##algo##_start(&tg.sg.g) == -1);

#define check_all_inner()			\
	do {					\
		check_one_inner(dfs);		\
		check_one_inner(bfs);		\
	} while (0)

#define check_one_outer(algo)					\
	do {							\
		ok1(aga_##algo##_start(&tg.sg.g) == 0);		\
		check_all_inner();				\
		aga_finish(&tg.sg.g);				\
	} while (0)

#define check_all_outer()			\
	do {					\
		check_one_outer(dfs);		\
		check_one_outer(bfs);		\
	} while (0)

int main(void)
{
	struct trivial_graph tg;

	if (NUM_ALGOS)
		plan_tests(NUM_ALGOS + NUM_ALGOS * NUM_ALGOS);
	else
		plan_skip_all("Nothing to test");

	trivial_graph_init(&tg);

	check_all_outer();

	/* This exits depending on whether all tests passed */
	return exit_status();
}
