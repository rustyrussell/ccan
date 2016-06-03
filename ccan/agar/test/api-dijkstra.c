#include "config.h"

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

#include <ccan/tap/tap.h>
#include <ccan/tal/tal.h>
#include <ccan/array_size/array_size.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static void test_trivial(void)
{
	struct agar_state *sr;
	aga_icost_t cost;
	const void *node;

	ok1(sr = agar_dijkstra_new(NULL, &trivial_graphr.gr, int2ptr(1)));
	ok1(agar_dijkstra_step(sr, &node));
	ok1(ptr2int(node) == 1);
	ok1(!agar_dijkstra_step(sr, &node));
	ok1(agar_dijkstra_path(sr, int2ptr(1), &cost, NULL, NULL));
	ok1(cost == 0);
	tal_free(sr);
}

static void test_parallel(void)
{
	struct parallel_graphr pgr;
	struct agar_state *sr;
	aga_icost_t cost;
	const void *node, *edge;

	parallel_graphr_init(&pgr, 3, 0);

	ok1(sr = agar_dijkstra_new(NULL, &pgr.gr, int2ptr(1)));
	ok1(agar_dijkstra_step(sr, &node));
	ok1(ptr2int(node) == 1);
	ok1(agar_dijkstra_step(sr, &node));
	ok1(ptr2int(node) == 2);
	ok1(!agar_dijkstra_step(sr, &node));
	ok1(agar_dijkstra_path(sr, int2ptr(1), &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(agar_dijkstra_path(sr, int2ptr(2), &cost, &node, NULL));
	ok1(cost == 2);
	ok1(node == int2ptr(1));
	tal_free(sr);

	ok1(sr = agar_dijkstra_new(NULL, &pgr.gr, int2ptr(2)));
	ok1(agar_dijkstra_step(sr, &node));
	ok1(ptr2int(node) == 2);
	ok1(!agar_dijkstra_step(sr, &node));
	ok1(agar_dijkstra_path(sr, int2ptr(2), &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(!agar_dijkstra_path(sr, int2ptr(1), NULL, NULL, NULL));
	tal_free(sr);

	parallel_graphr_init(&pgr, 3, 2);
	ok1(sr = agar_dijkstra_new(NULL, &pgr.gr, int2ptr(1)));
	ok1(agar_dijkstra_path(sr, int2ptr(2), &cost, &node, &edge));
	ok1(cost == 1);
	ok1(ptr2int(node) == 1);
	ok1(ptr2int(edge) == 2);
	tal_free(sr);
}

#define FULL_LEN	4

static void test_full(void)
{
	struct full_graphr fgr;
	int i, j;

	full_graphr_init(&fgr, FULL_LEN);

	for (i = 1; i <= FULL_LEN; i++) {
		struct agar_state *sr;

		ok1(sr = agar_dijkstra_new(NULL, &fgr.gr, int2ptr(i)));

		for (j = 1; j <= FULL_LEN; j++) {
			aga_icost_t cost;
			const void *node, *edge;

			ok1(agar_dijkstra_path(sr, int2ptr(j),
					      &cost, &node, &edge));
			if (i == j) {
				ok1(cost == 0);
			} else {
				ok1(cost == 1);
				ok1(node == int2ptr(i));
				ok1(edge == int2ptr(j));
			}
		}

		tal_free(sr);
	}
}

#define CHAIN_LEN	8

static void test_chain(void)
{
	struct chain_graphr cgr;
	int i, j;

	chain_graphr_init(&cgr, CHAIN_LEN);

	for (i = 1; i <= CHAIN_LEN; i++) {
		struct agar_state *sr;

		ok1(sr = agar_dijkstra_new(NULL, &cgr.fgr.gr, int2ptr(i)));

		for (j = 1; j <= CHAIN_LEN; j++) {
			aga_icost_t cost;

			ok1(agar_dijkstra_path(sr, int2ptr(j),
					      &cost, NULL, NULL));
			ok1(cost == labs(i - j));
		}

		tal_free(sr);
	}
}

static void test_error(void)
{
	struct agar_state *sr;
	aga_icost_t cost;

	ok1(sr = agar_dijkstra_new(NULL, &error_graphr.gr, int2ptr(1)));
	ok1(agar_dijkstra_path(sr, int2ptr(1), &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(agar_dijkstra_path(sr, int2ptr(2), &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(!agar_dijkstra_path(sr, int2ptr(3), &cost, NULL, NULL));
	ok1(!agar_dijkstra_path(sr, int2ptr(4), &cost, NULL, NULL));
	tal_free(sr);

	ok1(sr = agar_dijkstra_new(NULL, &error_graphr.gr, int2ptr(3)));
	ok1(agar_dijkstra_path(sr, int2ptr(3), &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(!agar_dijkstra_path(sr, int2ptr(4), &cost, NULL, NULL));
	ok1(agar_error(sr) == -1);
	tal_free(sr);
}

static void test_traversal1(void)
{
	struct agar_state *sr;
	aga_icost_t cost;

	/* This is mostly about testing we correctly handle
	 * non-reachable nodes */
	ok1(sr = agar_dijkstra_new(NULL, &traversal1_graphr.gr, int2ptr(1)));
	ok1(agar_dijkstra_path(sr, int2ptr(1),
			      &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(agar_dijkstra_path(sr, int2ptr(2),
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(agar_dijkstra_path(sr, int2ptr(3),
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(agar_dijkstra_path(sr, int2ptr(4),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(agar_dijkstra_path(sr, int2ptr(5),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(agar_dijkstra_path(sr, int2ptr(6),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(!agar_dijkstra_path(sr, int2ptr(7),
			       NULL, NULL, NULL));
	ok1(!agar_dijkstra_path(sr, int2ptr(8),
			       NULL, NULL, NULL));
	ok1(!agar_dijkstra_path(sr, int2ptr(9),
			       NULL, NULL, NULL));
	tal_free(sr);

	ok1(sr = agar_dijkstra_new(NULL, &traversal1_graphr.gr, int2ptr(9)));
	ok1(agar_dijkstra_path(sr, int2ptr(9),
			      &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(agar_dijkstra_path(sr, int2ptr(8),
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(agar_dijkstra_path(sr, int2ptr(7),
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(agar_dijkstra_path(sr, int2ptr(6),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(agar_dijkstra_path(sr, int2ptr(5),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(agar_dijkstra_path(sr, int2ptr(4),
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(!agar_dijkstra_path(sr, int2ptr(3),
			       NULL, NULL, NULL));
	ok1(!agar_dijkstra_path(sr, int2ptr(2),
			       NULL, NULL, NULL));
	ok1(!agar_dijkstra_path(sr, int2ptr(1),
			       NULL, NULL, NULL));
	tal_free(sr);
}

static void test_shortcut1(void)
{
	struct agar_state *sr;
	aga_icost_t cost;
	const void *node;

	ok1(sr = agar_dijkstra_new(NULL, &shortcut1_graphr.gr, int2ptr(1)));
	ok1(agar_dijkstra_path(sr, int2ptr(3), &cost, &node, NULL));
	ok1(cost == 2);
	ok1(node == int2ptr(2));
	ok1(agar_dijkstra_path(sr, int2ptr(2), &cost, &node, NULL));
	ok1(cost == 1);
	ok1(node == int2ptr(1));
	tal_free(sr);
}

static void test_shortcut2(void)
{
	struct agar_state *sr;

	ok1(sr = agar_dijkstra_new(NULL, &shortcut2_graphr.gr, int2ptr(1)));
	agar_dijkstra_all_paths(sr);
	ok1(agar_error(sr) == AGA_ERR_NEGATIVE_COST);
	tal_free(sr);
}

int main(void)
{
	plan_tests(6 + 23
		   + FULL_LEN * (FULL_LEN*4 - 1)
		   + CHAIN_LEN * (1 + CHAIN_LEN*2)
		   + 12 + 32 + 7 + 2);

	test_trivial();
	test_parallel();
	test_full();
	test_chain();
	test_error();
	test_traversal1();
	test_shortcut1();
	test_shortcut2();
	
	return exit_status();
}
