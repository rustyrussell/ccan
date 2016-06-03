#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static const void *trivial_first_edge_r(const struct agar_graph *g,
					const void *nr)
{
	assert(ptr2int(nr) == 1);
	return NULL;
}

static const void *trivial_next_edge_r(const struct agar_graph *gr,
				       const void *nr, const void *edge)
{
	assert(0);
}

static int trivial_edge_info_r(const struct agar_graph *gr,
			       const void *nr, const void *edge,
			       struct agar_edge_info *eir)
{
	assert(0);
}

struct trivial_graphr trivial_graphr = {
	AGAR_INIT_GRAPH(trivial_first_edge_r, trivial_next_edge_r,
			trivial_edge_info_r),
};
