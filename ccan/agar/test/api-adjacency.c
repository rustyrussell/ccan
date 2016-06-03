#include "config.h"

#include <stddef.h>
#include <assert.h>

#include <ccan/agar/agar.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/tap/tap.h>

#include "simple-graphr.h"

static void test_adjacency(const char *name,
			   const struct agar_graph *gr,
			   const struct adjacency_listr *atr)
{
	int i;

	for (i = 0; atr[i].from != 0; i++) {
		const void *e;
		struct agar_edge_info eir;
		int j = 0;
		int err;
		ptrint_t *from = int2ptr(atr[i].from);

		agar_for_each_edge_info(e, eir, err, gr, from) {
			const void *cmpto;

			assert(j < MAX_EDGES);
			cmpto = int2ptr(atr[i].to[j]);
			ok(cmpto == eir.to,
			   "%s: %p #%d -> #%ld (expected #%d -> #%d)", name, e,
			   atr[i].from, ptr2int(eir.to),
			   atr[i].from, atr[i].to[j]);

			j++;
		}
		if (atr[i].to[j] < 0) {
			ok(err == atr[i].to[j], "%s: %p #%d -> ERROR %d",
			   name, e, atr[i].from, atr[i].to[j]);
			continue; /* Move onto next node on errors */
		}
		assert(j < MAX_EDGES);
		ok(atr[i].to[j] == 0,
		   "%s: %p #%d -> --- (expected #%d -> #%d)", name, e,
		   atr[i].from, atr[i].from, atr[i].to[j]);
	}
}

int main(void)
{
	struct parallel_graphr pgr;
	struct full_graphr fgr;
	struct chain_graphr cgr;
	struct grid_graphr ggr1, ggr2;

	plan_tests(1 + 5 + 30 + 22 + 21 + 33 + 6 + 6 + 6);

	test_adjacency("trivial", &trivial_graphr.gr, trivial_adjacencyr);

	parallel_graphr_init(&pgr, 3, 0);
	test_adjacency("parallel nlinks 3", &pgr.gr,
		       parallel_adjacencyr_nlinks3);

	full_graphr_init(&fgr, 5);
	test_adjacency("full 5", &fgr.gr, full_adjacencyr_5);

	chain_graphr_init(&cgr, 8);
	test_adjacency("chain 8", &cgr.fgr.gr, chain_adjacencyr_8);

	grid_graphr_init(&ggr1, 3, 3, true, true, false, false);
	test_adjacency("grid 3x3 right-down", &ggr1.gr,
		       grid_adjacencyr_3x3_rightdown);

	grid_graphr_init(&ggr2, 3, 3, true, true, true, true);
	test_adjacency("grid 3x3 all", &ggr2.gr,
		       grid_adjacencyr_3x3_all);

	test_adjacency("error graph", &error_graphr.gr, error_adjacencyr);

	test_adjacency("shortcut1 graph", &shortcut1_graphr.gr,
		       shortcut1_adjacencyr);

	test_adjacency("shortcut2 graph", &shortcut2_graphr.gr,
		       shortcut2_adjacencyr);
	
	return exit_status();
}
