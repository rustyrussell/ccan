#include "config.h"

#include <ccan/aga/aga.h>
#include <ccan/aga/aga.c>

typedef struct edge edge_t;

#ifndef EDGE1
#define EDGE1 edge_t
#endif

#ifndef EDGE2
#define EDGE2 edge_t
#endif

#ifndef EDGE3
#define EDGE3 edge_t
#endif

#ifndef EDGE4
#define EDGE4 edge_t
#endif

int main(void)
{
	struct aga_graph g;
	EDGE1 *(*first_edge)(const struct aga_graph *g,
			     const struct aga_node *n) = NULL;
	EDGE2 *(*next_edge)(const struct aga_graph *g,
			    const struct aga_node *n,
			    EDGE3 *e) = NULL;
	int (*edge_info)(const struct aga_graph *g, const struct aga_node *n,
			 EDGE4 *e, struct aga_edge_info *ei) = NULL;

	aga_init_graph(&g, first_edge, next_edge, edge_info);

	return 0;
}

