#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static ptrint_t *negacycle_first_edge(const struct aga_graph *g,
				      const struct aga_node *n)
{
	return int2ptr(1);
}

static ptrint_t *negacycle_next_edge(const struct aga_graph *g,
				     const struct aga_node *n,
				     ptrint_t *e)
{
	assert(ptr2int(e) == 1);
	return NULL;
}

static int negacycle_edge_info(const struct aga_graph *g,
			       const struct aga_node *n,
			       ptrint_t *e, struct aga_edge_info *ei)
{
	struct negacycle_graph *ng = container_of(g, struct negacycle_graph,
						   sg.g);
	int ni = n - ng->sg.nodes;

	assert(ptr2int(e) == 1);
	ei->to = &ng->sg.nodes[(ni % 3) + 1];
	if (ni == 3)
		ei->icost = -3;
	return 0;
}

void negacycle_graph_init(struct negacycle_graph *ng)
{
	simple_graph_init(&ng->sg, negacycle_first_edge,
			  negacycle_next_edge,
			  negacycle_edge_info);
}
