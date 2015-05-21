#include <ccan/aga/aga.h>

#include "simple-graph.h"

void simple_graph_init_(struct simple_graph *sg)
{
	int i;

	for (i = 0; i < MAX_NODES; i++)
		aga_node_init(&sg->nodes[i]);
}
