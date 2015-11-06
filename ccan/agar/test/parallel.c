#include "config.h"

#include <assert.h>

#include <ccan/agar/agar.h>
#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include "simple-graphr.h"

static const void *parallel_first_edge_r(const struct agar_graph *gr,
					 const void *nr)
{
	const struct parallel_graphr *pgr
		= container_of(gr, struct parallel_graphr, gr);

	if (ptr2int(nr) != 1) {
		assert(ptr2int(nr) == 2);
		return NULL;
	}

	if (pgr->nlinks)
		return int2ptr(1);
	else
		return NULL;
}

static const void *parallel_next_edge_r(const struct agar_graph *gr,
					const void *nr, const void *edge)
{
	const struct parallel_graphr *pgr
		= container_of(gr, struct parallel_graphr, gr);
	int index = ptr2int(edge);

	if (ptr2int(nr) != 1) {
		assert(ptr2int(nr) == 2);
		return NULL;
	}

	if (index < pgr->nlinks)
		return int2ptr(index + 1);
	else
		return NULL;
}

static int parallel_edge_info_r(const struct agar_graph *gr,
				const void *nr, const void *edge,
				struct agar_edge_info *eir)
{
	const struct parallel_graphr *pgr
		= container_of(gr, struct parallel_graphr, gr);
	assert(ptr2int(nr) == 1);

	eir->to = int2ptr(2);
	if (ptr2int(edge) == pgr->cheaplink)
		eir->icost = 1;
	else
		eir->icost = 2;
	return 0;
}

void parallel_graphr_init(struct parallel_graphr *pgr, int nlinks,
			  int cheaplink)
{
	pgr->nlinks = nlinks;
	pgr->cheaplink = cheaplink;

	agar_init_graph(&pgr->gr, parallel_first_edge_r, parallel_next_edge_r,
			parallel_edge_info_r);
}
