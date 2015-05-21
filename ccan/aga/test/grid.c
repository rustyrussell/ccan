#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static ptrint_t *grid_first_edge(const struct aga_graph *g,
				 const struct aga_node *n)
{
	return int2ptr(1);
}

static ptrint_t *grid_next_edge(const struct aga_graph *g,
				const struct aga_node *n,
				ptrint_t *e)
{
	int index = ptr2int(e);

	if (index < 4)
		return int2ptr(index + 1);
	else
		return NULL;	       
}

static int grid_edge_info(const struct aga_graph *g,
			  const struct aga_node *n,
			  ptrint_t *e, struct aga_edge_info *ei)
{
	struct grid_graph *gg = container_of(g, struct grid_graph, sg.g);
	int ni = n - gg->sg.nodes;
	int x = ((ni - 1) % gg->nx) + 1;
	int y = ((ni - 1) / gg->nx) + 1;
	int i = ptr2int(e);

	assert((x >= 1) && (x <= gg->nx));
	assert((y >= 1) && (y <= gg->ny));

	switch (i) {
	case 1: /* right */
		if (gg->right && (x != gg->nx))
			ei->to = &gg->sg.nodes[ni + 1];
		break;

	case 2: /* down */
		if (gg->down && (y != gg->ny))
			ei->to = &gg->sg.nodes[ni + gg->nx];
		break;
		
	case 3: /* left */
		if (gg->left && (x != 1))
			ei->to = &gg->sg.nodes[ni - 1];
		break;

	case 4: /* up */
		if (gg->up && (y != 1))
			ei->to = &gg->sg.nodes[ni - gg->nx];
		break;

	default:
		assert(0);
	}
	return 0;
}

void grid_graph_init(struct grid_graph *gg, int nx, int ny,
		     bool right, bool down, bool left, bool up)
{
	assert((nx * ny) < MAX_NODES);

	gg->nx = nx;
	gg->ny = ny;
	gg->right = right;
	gg->down = down;
	gg->left = left;
	gg->up = up;

	simple_graph_init(&gg->sg, grid_first_edge, grid_next_edge,
			  grid_edge_info);
}
