#include "config.h"

#include <assert.h>

#include <ccan/aga/aga.h>
#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include "simple-graph.h"

static ptrint_t *parallel_first_edge(const struct aga_graph *g,
				       const struct aga_node *n)
{
	struct parallel_graph *pg = container_of(g, struct parallel_graph, sg.g);

	if (n != &pg->sg.nodes[1]) {
		assert(n == &pg->sg.nodes[2]);
		return NULL;
	}

	if (pg->nlinks)
		return int2ptr(1);
	else
		return NULL;
}

static ptrint_t *parallel_next_edge(const struct aga_graph *g,
				      const struct aga_node *n,
				      ptrint_t *edge)
{
	struct parallel_graph *pg = container_of(g, struct parallel_graph, sg.g);
	int index = ptr2int(edge);

	if (n != &pg->sg.nodes[1]) {
		assert(n == &pg->sg.nodes[2]);
		return NULL;
	}

	if (index < pg->nlinks)
		return int2ptr(index + 1);
	else
		return NULL;
}

static int parallel_edge_info(const struct aga_graph *g, const struct aga_node *n,
			      ptrint_t *edge, struct aga_edge_info *ei)
{
	struct parallel_graph *pg = container_of(g, struct parallel_graph, sg.g);

	assert(n == &pg->sg.nodes[1]);

	ei->to = &pg->sg.nodes[2];
	if (ptr2int(edge) == pg->cheaplink)
		ei->icost = 1;
	else
		ei->icost = 2;
	return 0;
}

void parallel_graph_init(struct parallel_graph *pg, int nlinks, int cheaplink)
{
	pg->nlinks = nlinks;
	pg->cheaplink = cheaplink;

	simple_graph_init(&pg->sg, parallel_first_edge, parallel_next_edge,
			  parallel_edge_info);
}
