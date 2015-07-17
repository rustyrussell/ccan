#include "config.h"

#include <stdlib.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static int chain_edge_info_r(const struct agar_graph *gr,
			     const void *nr, const void *e,
			     struct agar_edge_info *eir)
{
	int fromi = ptr2int(nr);
	int toi = ptr2int(e);

	if ((toi == fromi + 1) || (fromi == toi + 1))
		eir->to = int2ptr(toi);

	return 0;
}

void chain_graphr_init(struct chain_graphr *cgr, int nnodes)
{
	cgr->fgr.nnodes = nnodes;
	agar_init_graph(&cgr->fgr.gr, full_first_edge_r, full_next_edge_r,
			chain_edge_info_r);
}
