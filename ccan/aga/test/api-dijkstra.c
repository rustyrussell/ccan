#include "config.h"

#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

#include <ccan/tap/tap.h>
#include <ccan/array_size/array_size.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static void test_trivial(void)
{
	struct trivial_graph tg;
	aga_icost_t cost;
	struct aga_node *node;
	const void *edge;

	trivial_graph_init(&tg);

	ok1(aga_dijkstra_start(&tg.sg.g, &tg.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_step(&tg.sg.g) == &tg.sg.nodes[1]);
	ok1(aga_dijkstra_step(&tg.sg.g) == NULL);
	ok1(aga_dijkstra_path(&tg.sg.g, &tg.sg.nodes[1], &cost, &node, &edge));
	ok1(cost == 0);
	ok1(node == NULL);
	ok1(edge == NULL);
	aga_finish(&tg.sg.g);
}

static void test_parallel(void)
{
	struct parallel_graph pg;
	aga_icost_t cost;
	struct aga_node *node;
	const void *edge;

	parallel_graph_init(&pg, 3, 0);

	ok1(aga_dijkstra_start(&pg.sg.g, &pg.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_step(&pg.sg.g) == &pg.sg.nodes[1]);
	ok1(aga_dijkstra_step(&pg.sg.g) == &pg.sg.nodes[2]);
	ok1(aga_dijkstra_step(&pg.sg.g) == NULL);
	ok1(aga_dijkstra_path(&pg.sg.g, &pg.sg.nodes[1], &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(aga_dijkstra_path(&pg.sg.g, &pg.sg.nodes[2], &cost, &node, NULL));
	ok1(cost == 2);
	ok1(node == &pg.sg.nodes[1]);
	aga_finish(&pg.sg.g);

	ok1(aga_dijkstra_start(&pg.sg.g, &pg.sg.nodes[2]) == 0);
	ok1(aga_dijkstra_step(&pg.sg.g) == &pg.sg.nodes[2]);
	ok1(aga_dijkstra_step(&pg.sg.g) == NULL);
	ok1(aga_dijkstra_path(&pg.sg.g, &pg.sg.nodes[2], &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(!aga_dijkstra_path(&pg.sg.g, &pg.sg.nodes[1], NULL, NULL, NULL));
	aga_finish(&pg.sg.g);


	parallel_graph_init(&pg, 3, 2);
	ok1(aga_dijkstra_start(&pg.sg.g, &pg.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_path(&pg.sg.g, &pg.sg.nodes[2], &cost, &node, &edge));
	ok1(cost == 1);
	ok1(node == &pg.sg.nodes[1]);
	ok1(ptr2int(edge) == 2);
	aga_finish(&pg.sg.g);
}

#define FULL_LEN	4

static void test_full(void)
{
	struct full_graph fg;
	int i, j;

	full_graph_init(&fg, FULL_LEN);

	for (i = 1; i <= FULL_LEN; i++) {
		ok1(aga_dijkstra_start(&fg.sg.g, &fg.sg.nodes[i]) == 0);

		for (j = 1; j <= FULL_LEN; j++) {
			aga_icost_t cost;
			struct aga_node *node;
			const void *edge;

			ok1(aga_dijkstra_path(&fg.sg.g, &fg.sg.nodes[j],
					      &cost, &node, &edge));
			if (i == j) {
				ok1(cost == 0);
				ok1(node == NULL);
				ok1(edge == NULL);
			} else {
				ok1(cost == 1);
				ok1(node == &fg.sg.nodes[i]);
				ok1(edge == &fg.sg.nodes[j]);
			}
		}

		aga_finish(&fg.sg.g);
	}
}

#define CHAIN_LEN	8

static void test_chain(void)
{
	struct chain_graph cg;
	int i, j;

	chain_graph_init(&cg, CHAIN_LEN);

	for (i = 1; i <= CHAIN_LEN; i++) {
		ok1(aga_dijkstra_start(&cg.fg.sg.g, &cg.fg.sg.nodes[i]) == 0);

		for (j = 1; j <= CHAIN_LEN; j++) {
			aga_icost_t cost;

			ok1(aga_dijkstra_path(&cg.fg.sg.g, &cg.fg.sg.nodes[j],
					      &cost, NULL, NULL));
			ok1(cost == labs(i - j));
		}

		aga_finish(&cg.fg.sg.g);
	}
}

static void test_error(void)
{
	struct error_graph eg;
	aga_icost_t cost;

	error_graph_init(&eg);

	ok1(aga_dijkstra_start(&eg.sg.g, &eg.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[1], &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[2], &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(!aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[3], &cost, NULL, NULL));
	ok1(!aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[4], &cost, NULL, NULL));
	aga_finish(&eg.sg.g);

	ok1(aga_dijkstra_start(&eg.sg.g, &eg.sg.nodes[3]) == 0);
	ok1(aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[3], &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(!aga_dijkstra_path(&eg.sg.g, &eg.sg.nodes[4], &cost, NULL, NULL));
	ok1(aga_error(&eg.sg.g) == -1);
	aga_finish(&eg.sg.g);
}

static void test_traversal1(void)
{
	struct traversal1_graph t1g;
	aga_icost_t cost;

	/* This is mostly about testing we correctly handle
	 * non-reachable nodes */
	traversal1_graph_init(&t1g);

	ok1(aga_dijkstra_start(&t1g.sg.g, &t1g.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[1],
			      &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[2],
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[3],
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[4],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[5],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[6],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[7],
			       NULL, NULL, NULL));
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[8],
			       NULL, NULL, NULL));
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[9],
			       NULL, NULL, NULL));
	aga_finish(&t1g.sg.g);

	ok1(aga_dijkstra_start(&t1g.sg.g, &t1g.sg.nodes[9]) == 0);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[9],
			      &cost, NULL, NULL));
	ok1(cost == 0);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[8],
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[7],
			      &cost, NULL, NULL));
	ok1(cost == 1);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[6],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[5],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[4],
			      &cost, NULL, NULL));
	ok1(cost == 2);
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[3],
			       NULL, NULL, NULL));
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[2],
			       NULL, NULL, NULL));
	ok1(!aga_dijkstra_path(&t1g.sg.g, &t1g.sg.nodes[1],
			       NULL, NULL, NULL));
	aga_finish(&t1g.sg.g);
}

static void test_shortcut1(void)
{
	struct shortcut1_graph s1g;
	aga_icost_t cost;
	struct aga_node *node;

	shortcut1_graph_init(&s1g);

	ok1(aga_dijkstra_start(&s1g.sg.g, &s1g.sg.nodes[1]) == 0);
	ok1(aga_dijkstra_path(&s1g.sg.g, &s1g.sg.nodes[3],
			      &cost, &node, NULL));
	ok1(cost == 2);
	ok1(node == &s1g.sg.nodes[2]);
	ok1(aga_dijkstra_path(&s1g.sg.g, &s1g.sg.nodes[2],
			      &cost, &node, NULL));
	ok1(cost == 1);
	ok1(node == &s1g.sg.nodes[1]);
	aga_finish(&s1g.sg.g);
}

static void test_shortcut2(void)
{
	struct shortcut2_graph s2g;

	shortcut2_graph_init(&s2g);

	ok1(aga_dijkstra_start(&s2g.sg.g, &s2g.sg.nodes[1]) == 0);
	aga_dijkstra_all_paths(&s2g.sg.g);
	ok1(aga_error(&s2g.sg.g) == AGA_ERR_NEGATIVE_COST);
	aga_finish(&s2g.sg.g);
}

int main(void)
{
	plan_tests(7 + 20
		   + FULL_LEN * (1 + FULL_LEN*4)
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
