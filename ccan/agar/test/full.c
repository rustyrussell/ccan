#include "config.h"

#include <stdlib.h>
#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

const void *full_first_edge_r(const struct agar_graph *gr,
			      const void *nr)
{
	return int2ptr(1);
}

const void *full_next_edge_r(const struct agar_graph *gr,
			     const void *nr, const void *e)
{
	struct full_graphr *fgr = container_of(gr, struct full_graphr, gr);
	int ni = ptr2int(e);

	ni += 1;
	if (ni <= fgr->nnodes)
		return int2ptr(ni);
	else
		return NULL;
}

static int full_edge_info_r(const struct agar_graph *gr,
			    const void *nr, const void *edge,
			    struct agar_edge_info *eir)
{
	eir->to = edge;
	return 0;
}

void full_graphr_init(struct full_graphr *fgr, int nnodes)
{
	fgr->nnodes = nnodes;
	agar_init_graph(&fgr->gr, full_first_edge_r, full_next_edge_r,
			full_edge_info_r);
}
