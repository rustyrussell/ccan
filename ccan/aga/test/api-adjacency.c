#include "config.h"

#include <stddef.h>
#include <assert.h>

#include <ccan/aga/aga.h>

#include <ccan/tap/tap.h>

#include "simple-graph.h"

static void test_adjacency(const char *name,
			   const struct simple_graph *sg,
			   const struct adjacency_list *at)
{
	int i;

	for (i = 0; at[i].from != 0; i++) {
		const void *e;
		struct aga_edge_info ei;
		int j = 0;
		const struct aga_node *from;
		int err = 0xdeadbeef;

		assert(i < MAX_NODES);

		from = &sg->nodes[at[i].from];

		aga_for_each_edge_info(e, ei, err, &sg->g, from) {
			const struct aga_node *cmpto;

			assert(j < MAX_EDGES);
			cmpto = &sg->nodes[at[i].to[j]];
			ok(cmpto == ei.to,
			   "%s: %p #%d -> #%ld (expected #%d -> #%d)", name, e,
			   at[i].from, (ei.to - sg->nodes),
			   at[i].from, at[i].to[j]);

			j++;
		}
		if (at[i].to[j] < 0) {
			ok(err == at[i].to[j], "%s: %p #%d -> ERROR %d",
			   name, e, at[i].from, at[i].to[j]);
			continue; /* Move onto next node on errors */
		} else {
			ok1(err == 0);
		}
		assert(j < MAX_EDGES);
		ok(at[i].to[j] == 0,
		   "%s: %p #%d -> --- (expected #%d -> #%d)", name, e,
		   at[i].from, at[i].from, at[i].to[j]);
	}
}

int main(void)
{
	struct trivial_graph tg;
	struct parallel_graph pg;
	struct full_graph fg;
	struct chain_graph cg;
	struct grid_graph gg1, gg2;
	struct error_graph eg;
	struct traversal1_graph t1g;
	struct shortcut1_graph s1g;
	struct shortcut2_graph s2g;

	plan_tests(2 + 7 + 35 + 30 + 30 + 42 + 9 + 30 + 9 + 9);

	trivial_graph_init(&tg);
	test_adjacency("trivial", &tg.sg, trivial_adjacency);

	parallel_graph_init(&pg, 3, 0);
	test_adjacency("parallel nlinks 3", &pg.sg,
		       parallel_adjacency_nlinks3);

	full_graph_init(&fg, 5);
	test_adjacency("full 5", &fg.sg, full_adjacency_5);

	chain_graph_init(&cg, 8);
	test_adjacency("chain 8", &cg.fg.sg, chain_adjacency_8);

	grid_graph_init(&gg1, 3, 3, true, true, false, false);
	test_adjacency("grid 3x3 right-down", &gg1.sg,
		       grid_adjacency_3x3_rightdown);

	grid_graph_init(&gg2, 3, 3, true, true, true, true);
	test_adjacency("grid 3x3 all", &gg2.sg,
		       grid_adjacency_3x3_all);

	error_graph_init(&eg);
	test_adjacency("error graph", &eg.sg, error_adjacency);

	traversal1_graph_init(&t1g);
	test_adjacency("traversal1 graph", &t1g.sg, traversal1_adjacency);

	shortcut1_graph_init(&s1g);
	test_adjacency("shortcut1 graph", &s1g.sg, shortcut1_adjacency);

	shortcut2_graph_init(&s2g);
	test_adjacency("shortcut2 graph", &s2g.sg, shortcut2_adjacency);

	return exit_status();
}
