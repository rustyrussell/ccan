#include "config.h"

#include <stdlib.h>

#include <ccan/container_of/container_of.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static int chain_edge_info(const struct aga_graph *g,
			   const struct aga_node *node,
			   struct aga_node *edge,
			   struct aga_edge_info *ei)
{
	if ((edge == node + 1) || (node == edge + 1))
		ei->to = edge;

	return 0;
}

void chain_graph_init(struct chain_graph *cg, int nnodes)
{
	cg->fg.nnodes = nnodes;
	simple_graph_init(&cg->fg.sg, full_first_edge, full_next_edge,
			  chain_edge_info);
}
