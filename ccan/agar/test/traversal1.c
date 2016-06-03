#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static const void *traversal1_first_edge_r(const struct agar_graph *gr,
					   const void *nr)
{
	int ni = ptr2int(nr);

	switch (ni) {
	case 1:
	case 2:
	case 3:

	case 7:
	case 8:
	case 9:
		return int2ptr(1);

	case 4:
	case 5:
	case 6:
		return NULL;

	default:
		assert(0);
	}
}

static const void *traversal1_next_edge_r(const struct agar_graph *gr,
					  const void *nr, const void *e)
{
	int ni = ptr2int(nr);
	int index = ptr2int(e);

	assert((ni < 4) || (ni > 6));
	if (index == 1)
		return int2ptr(2);
	else if (index == 2)
		return NULL;
	else
		assert(0);
}

static int traversal1_edge_info_r(const struct agar_graph *gr,
				  const void *nr, const void *e,
				  struct agar_edge_info *eir)
{
	int ni = ptr2int(nr);
	int index = ptr2int(e);

	assert((index == 1) || (index == 2));

	switch (ni) {
	case 1:
		if (index == 1)
			eir->to = int2ptr(2);
		else
			eir->to = int2ptr(3);
		break;

	case 2:
		if (index == 1)
			eir->to = int2ptr(4);
		else
			eir->to = int2ptr(5);
		break;
	case 3:
		if (index == 1)
			eir->to = int2ptr(5);
		else
			eir->to = int2ptr(6);
		break;

	case 7:
		if (index == 1)
			eir->to = int2ptr(5);
		else
			eir->to = int2ptr(4);
		break;

	case 8:
		if (index == 1)
			eir->to = int2ptr(6);
		else
			eir->to = int2ptr(5);
		break;

	case 9:
		if (index == 1)
			eir->to = int2ptr(8);
		else
			eir->to = int2ptr(7);
		break;

	default:
		assert(0);
	}
	return 0;
}

struct traversal1_graphr traversal1_graphr = {
	AGAR_INIT_GRAPH(traversal1_first_edge_r, traversal1_next_edge_r,
			traversal1_edge_info_r),
};
