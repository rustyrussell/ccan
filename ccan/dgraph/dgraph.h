/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_DGRAPH_H
#define CCAN_DGRAPH_H
#include "config.h"
#include <ccan/tlist/tlist.h>
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <stdbool.h>

enum dgraph_dir {
	DGRAPH_FROM,
	DGRAPH_TO
};

/* strust tlist_dgraph_edge: a list of edges */
TLIST_TYPE(dgraph_edge, struct dgraph_edge);

/**
 * struct dgraph_node - a node in a directed graph.
 * @edge: edges indexed by enum dgraph_dir.
 *
 * edge[DGRAPH_FROM] edges have n[DGRAPH_FROM] == this.
 * edge[DGRAPH_TO] edges have n[DGRAPH_TO] == this.
 */
struct dgraph_node {
	struct tlist_dgraph_edge edge[2];
};

/**
 * struct dgraph_edge - an edge in a directed graph.
 * @list: our nodes's list of edges, indexed by enum dgraph_dir.
 * @n: the nodes, indexed by enum dgraph_dir.
 */
struct dgraph_edge {
	struct list_node list[2];
	struct dgraph_node *n[2];
};

void dgraph_init_node(struct dgraph_node *n);
void dgraph_clear_node(struct dgraph_node *n);
void dgraph_add_edge(struct dgraph_node *from, struct dgraph_node *to);
bool dgraph_del_edge(struct dgraph_node *from, struct dgraph_node *to);

/* You can dgraph_clear_node() the node you're on safely. */
#define dgraph_traverse_from(n, fn, arg)				\
	dgraph_traverse((n), DGRAPH_FROM,				\
			typesafe_cb_preargs(bool, void *, (fn), (arg),	\
					    struct dgraph_node *),	\
			(arg))

#define dgraph_traverse_to(n, fn, arg)					\
	dgraph_traverse((n), DGRAPH_TO,					\
			typesafe_cb_preargs(bool, void *, (fn), (arg),	\
					    struct dgraph_node *),	\
			(arg))

void dgraph_traverse(struct dgraph_node *n,
		     enum dgraph_dir dir,
		     bool (*fn)(struct dgraph_node *from, void *data),
		     const void *data);

#define dgraph_for_each_edge(n, i, dir)		\
	tlist_for_each(&(n)->edge[dir], i, list[dir])

#define dgraph_for_each_edge_safe(n, i, next, dir)		\
	tlist_for_each_safe(&(n)->edge[dir], i, next, list[dir])

#endif /* CCAN_DGRAPH_H */
