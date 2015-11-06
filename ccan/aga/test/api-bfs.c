#include "config.h"

#include <stddef.h>
#include <assert.h>

#include <ccan/tap/tap.h>
#include <ccan/array_size/array_size.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

#define test_bfs_partial(sg, first, ...)				\
	do {								\
		int cmp[] = { __VA_ARGS__ };				\
		bool stillok = true;					\
		struct aga_node *node;					\
		int i = 0;						\
		aga_bfs(node, &(sg)->g, &(sg)->nodes[first]) {	\
			int index = node - (sg)->nodes;			\
			diag("Visited %d\n", index);			\
			if (i >= ARRAY_SIZE(cmp) || (index != cmp[i]))	\
				stillok = false;			\
			i++;						\
		}							\
		ok1(stillok);						\
	} while (0)

#define test_bfs(sg, first, ...)					\
	do {								\
		ok1(aga_bfs_start(&(sg)->g) == 0);			\
		test_bfs_partial(sg, first, __VA_ARGS__);		\
		aga_finish(&(sg)->g);					\
	} while (0)

int main(void)
{
	struct trivial_graph tg;
	struct parallel_graph pg;
	struct full_graph fg;
	struct chain_graph cg;
	struct grid_graph gg1, gg2;
	struct error_graph eg;
	struct traversal1_graph t1g;
	struct aga_node *node;
	
	plan_tests(2 * 13 + 10 + 10);

	trivial_graph_init(&tg);
	test_bfs(&tg.sg, 1, 1);

	parallel_graph_init(&pg, 3, 0);
	test_bfs(&pg.sg, 1, 1, 2);

	full_graph_init(&fg, 5);
	test_bfs(&fg.sg, 1, 1, 2, 3, 4, 5);
	test_bfs(&fg.sg, 3, 3, 1, 2, 4, 5);

	chain_graph_init(&cg, 8);
	test_bfs(&cg.fg.sg, 1, 1, 2, 3, 4, 5, 6, 7, 8);
	test_bfs(&cg.fg.sg, 8, 8, 7, 6, 5, 4, 3, 2, 1);
	test_bfs(&cg.fg.sg, 5, 5, 4, 6, 3, 7, 2, 8, 1);

	grid_graph_init(&gg1, 3, 3, true, true, false, false);
	test_bfs(&gg1.sg, 1, 1, 2, 4, 3, 5, 7, 6, 8, 9);
	test_bfs(&gg1.sg, 5, 5, 6, 8, 9);
	test_bfs(&gg1.sg, 9, 9);

	grid_graph_init(&gg2, 3, 3, true, true, true, true);
	test_bfs(&gg2.sg, 1, 1, 2, 4, 3, 5, 7, 6, 8, 9);
	test_bfs(&gg2.sg, 5, 5, 6, 8, 4, 2, 9, 3, 7, 1);
	test_bfs(&gg2.sg, 9, 9, 8, 6, 7, 5, 3, 4, 2, 1);

	error_graph_init(&eg);
	test_bfs(&eg.sg, 1, 1, 2);
	ok(aga_bfs_start(&eg.sg.g) == 0, "started error traversal");
	node = aga_bfs_explore(&eg.sg.g, &eg.sg.nodes[3]);
	ok(node == &eg.sg.nodes[3], "Expected node #3 (%p), actually #%ld (%p)",
	   &eg.sg.nodes[3], node - eg.sg.nodes, node);
	node = aga_bfs_explore(&eg.sg.g, node);
	ok(node == &eg.sg.nodes[4], "Expected node #4 (%p), actually #%ld (%p)",
	   &eg.sg.nodes[4], node - eg.sg.nodes, node);
	ok1(aga_bfs_explore(&eg.sg.g, node) == NULL);
	ok1(aga_error(&eg.sg.g) == -1);
	ok1(aga_bfs_explore(&eg.sg.g, node) == NULL);
	aga_finish(&eg.sg.g);
	test_bfs(&eg.sg, 1, 1, 2);

	traversal1_graph_init(&t1g);
	test_bfs(&t1g.sg, 1, 1, 2, 3, 4, 5, 6);
	test_bfs(&t1g.sg, 9, 9, 8, 7, 6, 5, 4);

	ok1(aga_bfs_start(&t1g.sg.g) == 0);
	test_bfs_partial(&t1g.sg, 1, 1, 2, 3, 4, 5, 6);
	test_bfs_partial(&t1g.sg, 9, 9, 8, 7);
	aga_finish(&t1g.sg.g);

	ok1(aga_bfs_start(&t1g.sg.g) == 0);
	test_bfs_partial(&t1g.sg, 9, 9, 8, 7, 6, 5, 4);
	test_bfs_partial(&t1g.sg, 1, 1, 2, 3);
	aga_finish(&t1g.sg.g);

	return exit_status();
}
