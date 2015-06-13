/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_AGA_H
#define CCAN_AGA_H
/*
 * Abstract Graph Algorithms
 *
 * This module implements several standard algorithms on "abstract"
 * (directed) graphs.  That is to say rather than requiring a specific
 * concrete representation of the graph, user-supplied callbacks allow
 * the graph to be constructed as it is explored.
 *
 *
 * Node representation
 * ===================
 *
 * Graph nodes are represented by 'struct aga_node'
 *
 * - These will often be embedded in a caller-specific structure
 *   (calling code can then locate its own structures using
 *   container_of)
 *
 * - Nodes are semi-persistent - they MAY be constructed on the fly by
 *   the edge_info callback (see below), but MUST then remain in place
 *   at least as long as the completion of the current graph
 *   algorithm.
 *
 * - Nodes must be initialized with aga_node_init(), either up front,
 *   or as they are constructed on the fly.
 *
 * - The contents of the structure should be treated as opaque by the
 *   caller.
 *
 *
 * Edge representation
 * ===================
 *
 * Graph edges are reported by three caller supplied functions,
 * 'first_edge', 'next_edge' and 'edge_info'.
 *
 * - Edges are identified by a (void *)
 * - The combination of a graph, a node and this (void *) MUST
 *   uniquely identify an edge
 * - Different edges leading from different nodes MAY have the same
 *   (void *) identifier
 * - NULL has a special meaning (indicating there are no more edges
 *   from a given node).
 * - Otherwise, edge identifiers are treated as opaque by aga
 *
 * - Calling first_edge, followed by next_edge repeatedly must iterate
 *   through all the edges leading from node n.
 *
 * - Either first_edge or next_edge may return NULL indicating there
 *   are no further edges from this node
 *
 * - edge_info MAY return a negative value in case of error.  This
 *   will generally abort any aga algorithm which encounters it.
 *
 * - Otherwise edge_info must return 0.  Any other return value will
 *   cause an abort().
 *
 * - edge_info MAY set ei->to to NULL, indicating a "missing" edge,
 *   thus there MAY be more edge identifiers than actual edges from a
 *   given node.  Otherwise, edge_info MUST fill in the ei->to field
 *   with a pointer to the destination node of the given edge
 *
 * - The ei->to field for a returned edge MAY point to an existing
 *   struct aga_node, or it MAY have just been allocated by the edge
 *   callback itself.  If the latter, it MUST have been initialized
 *   with aga_node_init() before the edge callback returns.
 *
 * - If a node is contructed by the edge callback, any subsequent
 *   reference to that node by the edge callback for *any* node and
 *   index MUST use the same pointer.
 *
 * Concurrency
 * ===========
 *
 * - Because the algorithms implemented here keep state in the
 *   aga_node structures, only one algorithm can run at a time.
 *   Global state for algorithms is stored in the aga_graph structure.
 *
 * - When you start an algorithm (aga_*_start()) the graph is marked
 *   as in-use.
 *
 * - Subsequent attempts to start an algorithm will fail;
 *   aga_*_start() will return -1.
 *
 * - To release the graph for another algorithm, use aga_finish().
 *
 * - Calling functions associated with one algorithm while another is
 *   running has undefined results.
 *
 * Errors
 * ======
 *
 * - Errors may be reported by the edge_info callback, or may be
 *   detected internally by algorithms.
 *
 * - Algorithms will generally stop running when they encounter an
 *   error; the call which detects the error and subsequent calls will
 *   return a "safe", but otherwise meaningless value.
 *
 * - After an error is encountered aga_error() will return a non-zero
 *   value.  Negative values are reserved for errors reported by the
 *   user supplied edge callback.  Positive values are reserved for
 *   errors detected interally by aga.
 *
 * - Errors are cleared on aga_finish().
 */
#include "config.h"
#include <string.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/check_type/check_type.h>
#include <ccan/lstack/lstack.h>
#include <ccan/lqueue/lqueue.h>

struct aga_graph;
struct aga_node;

/*
 * Callbacks
 */
typedef const void *(*aga_first_edge_fn)(const struct aga_graph *g,
					 const struct aga_node *n);
typedef const void *(*aga_next_edge_fn)(const struct aga_graph *g,
					const struct aga_node *n,
					const void *e);

struct aga_edge_info {
	struct aga_node *to;
};
typedef int (*aga_edge_info_fn)(const struct aga_graph *g,
				const struct aga_node *n,
				const void *e, struct aga_edge_info *ei);

/*
 * Internal data structures
 */

struct aga_node {
	int sequence;
	union {
		struct {
			struct lstack_link parent;
			const void *edge;
		} dfs;
		struct {
			struct lqueue_link next;
			const void *edge;
		} bfs;
	} u;
};

struct aga_graph {
	int sequence;
	int error;

	aga_first_edge_fn first_edge;
	aga_next_edge_fn next_edge;
	aga_edge_info_fn edge_info;
};

/*
 * Core functions
 */

void aga_init_graph_(struct aga_graph *g,
		     aga_first_edge_fn first_edge,
		     aga_next_edge_fn next_edge,
		     aga_edge_info_fn edge_info);
#define aga_init_graph(g_, fefn_, nefn_, eifn_)				\
	do {								\
		struct aga_node *n_;					\
		struct aga_edge_info *ei_;				\
		BUILD_ASSERT(check_types_match((fefn_)((g_), n_),	\
					       (nefn_)((g_), n_,	\
						       (fefn_)((g_), n_))) \
			     == 0);					\
		BUILD_ASSERT(check_type((eifn_)((g_), n_,		\
						(fefn_)((g_), n_), ei_), \
					int) == 0);			\
		aga_init_graph_((g_), (aga_first_edge_fn)(fefn_),	\
				(aga_next_edge_fn)(nefn_),		\
				(aga_edge_info_fn)(eifn_));		\
	} while (0)

int aga_error(const struct aga_graph *g);

static inline void aga_node_init(struct aga_node *node)
{
	memset(node, 0, sizeof(*node));
}

void aga_finish(struct aga_graph *g);

const void *aga_first_edge(const struct aga_graph *g, const struct aga_node *n);
const void *aga_next_edge(const struct aga_graph *g, const struct aga_node *n,
			  const void *e);
int aga_edge_info(const struct aga_graph *g, const struct aga_node *n,
		  const void *e, struct aga_edge_info *ei);

#define aga_for_each_edge(_e, _g, _n)					\
	for ((_e) = aga_first_edge((_g), (_n)); (_e);			\
	     (_e) = aga_next_edge((_g), (_n), (_e)))

#define aga_for_each_edge_info(_e, _ei, _err, _g, _n)			\
	for ((_e) = aga_first_edge((_g), (_n));				\
	     (_e) && ((((_err) = aga_edge_info((_g), (_n), (_e), &(_ei)))) == 0); \
	     (_e) = aga_next_edge((_g), (_n), (_e)))			\
		if ((_ei).to)

/*
 * Depth first search
 */

int aga_dfs_start(struct aga_graph *g);
struct aga_node *aga_dfs_explore(struct aga_graph *g, struct aga_node *n);

#define aga_dfs(_n, _g, _start)					\
	for ((_n) = (_start); ((_n) = aga_dfs_explore((_g), (_n))) != NULL; )


/*
 * Breadth first search
 */

int aga_bfs_start(struct aga_graph *g);
struct aga_node *aga_bfs_explore(struct aga_graph *g, struct aga_node *n);

#define aga_bfs(_n, _g, _start)					\
	for ((_n) = (_start) ; ((_n) = aga_bfs_explore((_g), (_n))) != NULL; )

#endif /* CCAN_AGA_H */
