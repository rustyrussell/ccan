/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include <ccan/dgraph/dgraph.h>
#include <stdlib.h>

void dgraph_init_node(struct dgraph_node *n)
{
	tlist_init(&n->edge[DGRAPH_FROM]);
	tlist_init(&n->edge[DGRAPH_TO]);
}

static void free_edge(struct dgraph_edge *e)
{
	tlist_del_from(&e->n[DGRAPH_FROM]->edge[DGRAPH_FROM],
		       e, list[DGRAPH_FROM]);
	tlist_del_from(&e->n[DGRAPH_TO]->edge[DGRAPH_TO],
		       e, list[DGRAPH_TO]);
	free(e);
}

void dgraph_clear_node(struct dgraph_node *n)
{
	struct dgraph_edge *e;
	unsigned int i;

	for (i = DGRAPH_FROM; i <= DGRAPH_TO; i++) {
		while ((e = tlist_top(&n->edge[i], list[i])) != NULL) {
			assert(e->n[i] == n);
			free_edge(e);
		}
	}
}

void dgraph_add_edge(struct dgraph_node *from, struct dgraph_node *to)
{
	struct dgraph_edge *e = malloc(sizeof(*e));
	e->n[DGRAPH_FROM] = from;
	e->n[DGRAPH_TO] = to;
	tlist_add(&from->edge[DGRAPH_FROM], e, list[DGRAPH_FROM]);
	tlist_add(&to->edge[DGRAPH_TO], e, list[DGRAPH_TO]);
}

bool dgraph_del_edge(struct dgraph_node *from, struct dgraph_node *to)
{
	struct dgraph_edge *e, *next;

	dgraph_for_each_edge_safe(from, e, next, DGRAPH_FROM) {
		if (e->n[DGRAPH_TO] == to) {
			free_edge(e);
			return true;
		}
	}
	return false;
}

static bool traverse_depth_first(struct dgraph_node *n,
				 enum dgraph_dir dir,
				 bool (*fn)(struct dgraph_node *, void *),
				 const void *data)
{
	struct dgraph_edge *e, *next;

	dgraph_for_each_edge_safe(n, e, next, dir) {
		if (!traverse_depth_first(e->n[!dir], dir, fn, data))
			return false;
	}
	return fn(n, (void *)data);
}

void dgraph_traverse(struct dgraph_node *n,
		     enum dgraph_dir dir,
		     bool (*fn)(struct dgraph_node *, void *),
		     const void *data)
{
	struct dgraph_edge *e, *next;

	dgraph_for_each_edge_safe(n, e, next, dir) {
		if (!traverse_depth_first(e->n[!dir], dir, fn, data))
			break;
	}
}
