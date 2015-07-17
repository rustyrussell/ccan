#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/agar/agar.h>

#include "simple-graphr.h"

static const void *grid_first_edge_r(const struct agar_graph *gr,
				     const void *nr)
{
	return int2ptr(1);
}

static const void *grid_next_edge_r(const struct agar_graph *gr,
				    const void *nr, const void *e)
{
	int index = ptr2int(e);

	if (index < 4)
		return int2ptr(index + 1);
	else
		return NULL;	       
}

static int grid_edge_info_r(const struct agar_graph *gr,
			    const void *nr, const void *e,
			    struct agar_edge_info *eir)
{
	struct grid_graphr *ggr = container_of(gr, struct grid_graphr, gr);
	int ni = ptr2int(nr);
	int x = ((ni - 1) % ggr->nx) + 1;
	int y = ((ni - 1) / ggr->nx) + 1;
	int i = ptr2int(e);

	assert((x >= 1) && (x <= ggr->nx));
	assert((y >= 1) && (y <= ggr->ny));

	switch (i) {
	case 1: /* right */
		if (ggr->right && (x != ggr->nx))
			eir->to = int2ptr(ni + 1);
		break;

	case 2: /* down */
		if (ggr->down && (y != ggr->ny))
			eir->to = int2ptr(ni + ggr->nx);
		break;
		
	case 3: /* left */
		if (ggr->left && (x != 1))
			eir->to = int2ptr(ni - 1);
		break;

	case 4: /* up */
		if (ggr->up && (y != 1))
			eir->to = int2ptr(ni - ggr->nx);
		break;

	default:
		assert(0);
	}
	return 0;
}

void grid_graphr_init(struct grid_graphr *ggr, int nx, int ny,
		     bool right, bool down, bool left, bool up)
{
	ggr->nx = nx;
	ggr->ny = ny;
	ggr->right = right;
	ggr->down = down;
	ggr->left = left;
	ggr->up = up;

	agar_init_graph(&ggr->gr, grid_first_edge_r, grid_next_edge_r,
			grid_edge_info_r);
}
