#include "config.h"

#include <stdlib.h>
#include <assert.h>

#include <ccan/container_of/container_of.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

struct aga_node *full_first_edge(const struct aga_graph *g,
				 const struct aga_node *node)
{
	struct full_graph *fg = container_of(g, struct full_graph, sg.g);

	return &fg->sg.nodes[1];
}

struct aga_node *full_next_edge(const struct aga_graph *g,
				const struct aga_node *node,
				struct aga_node *edge)
{
	struct full_graph *fg = container_of(g, struct full_graph, sg.g);
	int index = (edge - fg->sg.nodes);

	if (index < fg->nnodes)
		return &fg->sg.nodes[index + 1];
	else
		return NULL;
}

static int full_edge_info(const struct aga_graph *g,
			  const struct aga_node *node,
			  struct aga_node *edge,
			  struct aga_edge_info *ei)
{
	ei->to = edge;
	return 0;
}

void full_graph_init(struct full_graph *fg, int nnodes)
{
	assert(nnodes < MAX_NODES);
	
	fg->nnodes = nnodes;
	simple_graph_init(&fg->sg, full_first_edge, full_next_edge,
			  full_edge_info);
}
