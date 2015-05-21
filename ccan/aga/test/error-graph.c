#include "config.h"

#include <assert.h>

#include <ccan/aga/aga.h>
#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include "simple-graph.h"

static ptrint_t *error_first_edge(const struct aga_graph *g,
				  const struct aga_node *n)
{
	return int2ptr(1);
}

static ptrint_t *error_next_edge(const struct aga_graph *g,
				 const struct aga_node *n,
				 ptrint_t *edge)
{
	assert(edge == int2ptr(1));

	return NULL;
}

static int error_edge_info(const struct aga_graph *g, const struct aga_node *n,
			   ptrint_t *edge, struct aga_edge_info *ei)
{
	struct error_graph *eg = container_of(g, struct error_graph, sg.g);
	int fromindex = n - eg->sg.nodes;

	switch (fromindex) {
	case 1:
		ei->to = &eg->sg.nodes[2];
		break;

	case 2:
		ei->to = NULL;
		break;

	case 3:
		ei->to = &eg->sg.nodes[4];
		break;

	default:
		return -1;
	}

	return 0;
}

void error_graph_init(struct error_graph *eg)
{
	simple_graph_init(&eg->sg, error_first_edge, error_next_edge,
			  error_edge_info);
}
