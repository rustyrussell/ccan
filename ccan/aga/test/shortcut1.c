#include "config.h"

#include <assert.h>

#include <ccan/container_of/container_of.h>
#include <ccan/ptrint/ptrint.h>

#include <ccan/aga/aga.h>

#include "simple-graph.h"

static ptrint_t *shortcut1_first_edge(const struct aga_graph *g,
				      const struct aga_node *n)
{
	struct shortcut1_graph *s1g = container_of(g, struct shortcut1_graph,
						   sg.g);
	int ni = n - s1g->sg.nodes;

	switch (ni) {
	case 1:
	case 2:
		return int2ptr(1);

	case 3:
		return NULL;

	default:
		assert(0);
	}
}

static ptrint_t *shortcut1_next_edge(const struct aga_graph *g,
				     const struct aga_node *n,
				     ptrint_t *e)
{
	struct shortcut1_graph *s1g = container_of(g, struct shortcut1_graph,
						   sg.g);
	int ni = n - s1g->sg.nodes;
	int index = ptr2int(e);

	switch (ni) {
	case 1:
		if (index == 1)
			return int2ptr(2);
		assert(index == 2);
		return NULL;

	case 2:
		assert(index == 1);
		return NULL;

	default:
		assert(0);
	}
}

static int shortcut1_edge_info(const struct aga_graph *g,
			       const struct aga_node *n,
			       ptrint_t *e, struct aga_edge_info *ei)
{
	struct shortcut1_graph *s1g = container_of(g, struct shortcut1_graph,
						   sg.g);
	int ni = n - s1g->sg.nodes;
	int index = ptr2int(e);

	switch (ni) {
	case 1:
		if (index == 1) {
			ei->to = &s1g->sg.nodes[3];
			ei->icost = 3;
		} else {
			assert(index == 2);
			ei->to = &s1g->sg.nodes[2];
		}
		break;

	case 2:
		assert(index == 1);
		ei->to = &s1g->sg.nodes[3];
		break;

	default:
		assert(0);
	}
	return 0;
}

void shortcut1_graph_init(struct shortcut1_graph *s1g)
{
	simple_graph_init(&s1g->sg, shortcut1_first_edge,
			  shortcut1_next_edge,
			  shortcut1_edge_info);
}
