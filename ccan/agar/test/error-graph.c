#include "config.h"

#include <assert.h>

#include <ccan/agar/agar.h>
#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include "simple-graphr.h"

static const void *error_first_edge_r(const struct agar_graph *gr,
				      const void *nr)
{
	return int2ptr(1);
}

static const void *error_next_edge_r(const struct agar_graph *gr,
				     const void *nr, const void *e)
{
	assert(ptr2int(e) == 1);

	return NULL;
}

static int error_edge_info_r(const struct agar_graph *gr,
			     const void *nr, const void *e,
			     struct agar_edge_info *eir)
{
	int fromindex = ptr2int(nr);

	switch (fromindex) {
	case 1:
		eir->to = int2ptr(2);
		break;

	case 2:
		eir->to = NULL;
		break;

	case 3:
		eir->to = int2ptr(4);
		break;

	default:
		return -1;
	}

	return 0;
}

struct error_graphr error_graphr = {
	AGAR_INIT_GRAPH(error_first_edge_r, error_next_edge_r,
			error_edge_info_r),
};
