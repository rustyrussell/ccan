#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static const void *negacycle_first_edge_r(const struct agar_graph *gr,
					  const void *nr)
{
	return int2ptr(1);
}

static const void *negacycle_next_edge_r(const struct agar_graph *gr,
					 const void *nr, const void *e)
{
	assert(ptr2int(e) == 1);
	return NULL;
}

static int negacycle_edge_info_r(const struct agar_graph *gr,
				 const void *nr, const void *e,
				 struct agar_edge_info *eir)
{
	int ni = ptr2int(nr);

	assert(ptr2int(e) == 1);
	eir->to = int2ptr((ni % 3) + 1);
	if (ni == 3)
		eir->icost = -3;
	return 0;
}

struct negacycle_graphr negacycle_graphr = {
	AGAR_INIT_GRAPH(negacycle_first_edge_r,
			negacycle_next_edge_r,
			negacycle_edge_info_r),
};
