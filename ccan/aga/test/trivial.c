#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static const void *trivial_first_edge(const struct aga_graph *g,
				      const struct aga_node *node)
{
	struct trivial_graph *tg = container_of(g, struct trivial_graph, sg.g);

	assert(node == &tg->sg.nodes[1]);
	return NULL;
}

static const void *trivial_next_edge(const struct aga_graph *g,
				     const struct aga_node *node,
				     const void *edge)
{
	assert(0);
}

static int trivial_edge_info(const struct aga_graph *g,
			     const struct aga_node *node,
			     const void *edge,
			     struct aga_edge_info *ei)
{
	assert(0);
}

void trivial_graph_init(struct trivial_graph *tg)
{
	simple_graph_init(&tg->sg, trivial_first_edge, trivial_next_edge,
			  trivial_edge_info);
}
