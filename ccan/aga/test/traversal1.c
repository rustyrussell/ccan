#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static ptrint_t *traversal1_first_edge(const struct aga_graph *g,
				       const struct aga_node *n)
{
	struct traversal1_graph *t1g = container_of(g, struct traversal1_graph,
						    sg.g);
	int ni = n - t1g->sg.nodes;

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

static ptrint_t *traversal1_next_edge(const struct aga_graph *g,
				      const struct aga_node *n,
				      ptrint_t *e)
{
	struct traversal1_graph *t1g = container_of(g, struct traversal1_graph,
						    sg.g);
	int ni = n - t1g->sg.nodes;
	int index = ptr2int(e);

	assert((ni < 4) || (ni > 6));
	if (index == 1)
		return int2ptr(2);
	else if (index == 2)
		return NULL;
	else
		assert(0);
}

static int traversal1_edge_info(const struct aga_graph *g,
				const struct aga_node *n,
				ptrint_t *e, struct aga_edge_info *ei)
{
	struct traversal1_graph *t1g = container_of(g, struct traversal1_graph,
						    sg.g);
	int ni = n - t1g->sg.nodes;
	int index = ptr2int(e);

	assert((index == 1) || (index == 2));

	switch (ni) {
	case 1:
		if (index == 1)
			ei->to = &t1g->sg.nodes[2];
		else
			ei->to = &t1g->sg.nodes[3];
		break;

	case 2:
		if (index == 1)
			ei->to = &t1g->sg.nodes[4];
		else
			ei->to = &t1g->sg.nodes[5];
		break;
	case 3:
		if (index == 1)
			ei->to = &t1g->sg.nodes[5];
		else
			ei->to = &t1g->sg.nodes[6];
		break;

	case 7:
		if (index == 1)
			ei->to = &t1g->sg.nodes[5];
		else
			ei->to = &t1g->sg.nodes[4];
		break;

	case 8:
		if (index == 1)
			ei->to = &t1g->sg.nodes[6];
		else
			ei->to = &t1g->sg.nodes[5];
		break;

	case 9:
		if (index == 1)
			ei->to = &t1g->sg.nodes[8];
		else
			ei->to = &t1g->sg.nodes[7];
		break;

	default:
		assert(0);
	}
	return 0;
}

void traversal1_graph_init(struct traversal1_graph *t1g)
{
	simple_graph_init(&t1g->sg, traversal1_first_edge,
			  traversal1_next_edge,
			  traversal1_edge_info);
}
