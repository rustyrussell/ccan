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
 * - The ei->icost field MAY be set by the edge_info callback to
 *   indicate the edge's cost / length represented as an integer (of
 *   type aga_icost_t),  Otherwise the cost defaults to 1.
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
#include <ccan/lpq/lpq.h>

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

typedef long aga_icost_t;

struct aga_edge_info {
	struct aga_node *to;
	aga_icost_t icost; /* integer edge cost */
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
		struct {
			aga_icost_t distance;
			struct aga_node *prev;
			const void *prevedge;
			bool complete;
			struct lpq_link pqlink;
		} dijkstra;
	} u;
};

struct aga_graph {
	int sequence;
	int error;

	aga_first_edge_fn first_edge;
	aga_next_edge_fn next_edge;
	aga_edge_info_fn edge_info;
	union {
		LPQ(struct aga_node, u.dijkstra.pqlink) dijkstra;
	} state;
};

/*
 * Core functions
 */

/**
 * aga_init_graph - Initialize a new abstract graph
 * @g: graph structure to initialize
 * @first_edge: first edge callback
 * @next_edge: next edge callback
 * @edge_into: edge info callback
 *
 * Initialize @g to represent an abstract graph defined by the
 * supplied edge callbacks
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

/**
 * enum aga_error - Error codes for aga routines
 *
 * These error codes are returned by aga_error() for errors detected
 * within aga itself (rather than errors reported by supplied
 * callbacks, which should be negative
 */
enum aga_error {
	/* No error */
	AGA_ERR_NONE = 0,
	/* Negative edge cost (in a context where it's not valid) */
	AGA_ERR_NEGATIVE_COST = 1,
};

/**
 * aga_error - Determine error state of a graph
 * @g: the graph
 *
 * Returns 0 if the graph is not in an error state, negative values
 * for error states reported by one of the edge callbacks and
 * postitive values for errors detected by aga itself.
 */
int aga_error(const struct aga_graph *g);

/**
 * aga_node_init - Initialize a graph node
 * @node: a graph node
 *
 * Initialize @node as a new graph node.  This must be called before
 * @node is passed to any aga function, or returned from an edge_info
 * callback (in the ei->to field)
 */
static inline void aga_node_init(struct aga_node *node)
{
	memset(node, 0, sizeof(*node));
}

/**
 * aga_finish - Finish an aga algorithm
 * @g: graph
 *
 * Wraps up the aga algorithm currently running on @g.  This will
 * clear any error conditions.  After this is called it is an error to
 * call aga functions on @g apart from aga_*_start() and aga_error.
 */
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
	for ((_err) = 0, (_e) = aga_first_edge((_g), (_n));		\
	     (_e) && ((((_err) = aga_edge_info((_g), (_n), (_e), &(_ei)))) == 0); \
	     (_e) = aga_next_edge((_g), (_n), (_e)))			\
		if ((_ei).to)

/*
 * Depth first search
 */

/**
 * aga_dfs_start - Start a depth-first search
 * @g: graph to search
 *
 * Begins the depth-first search algorithm on @g
 */
int aga_dfs_start(struct aga_graph *g);

/**
 * aga_dfs_explore - One step of depth-first search
 * @g: graph to search
 * @n: node to start exploration from
 *
 * If @n has not yet been explored since aga_dfs_start(), returns @n.
 * Otherwise returns the next node after @n in depth-first search
 * order.  Marks the returned node as explored.
 */
struct aga_node *aga_dfs_explore(struct aga_graph *g, struct aga_node *n);

/**
 * aga_dfs - Depth-first search
 * @_n: pointer to current node (output)
 * @_g: graph to search
 * @_start: node to start from
 *
 * Performs a depth first search.  The block following this macro is
 * executed with @_n set first to @_start, then to each node reachable
 * from @_start in depth first search order.
 *
 * aga_dfs_start() must be called before this macro is used.
 */
#define aga_dfs(_n, _g, _start)					\
	for ((_n) = (_start); ((_n) = aga_dfs_explore((_g), (_n))) != NULL; )


/*
 * Breadth first search
 */

/**
 * aga_bfs_start - Start a breadth-first search
 * @g: graph to search
 *
 * Begins the breadth-first search algorithm on @g
 */
int aga_bfs_start(struct aga_graph *g);

/**
 * aga_bfs_explore - One step of breadth-first search
 * @g: graph to search
 * @n: node to start exploration from
 *
 * If @n has not yet been explored since aga_bfs_start(), returns @n.
 * Otherwise returns the next node after @n in breadth-first search
 * order.  Marks the returned node as explored.
 */
struct aga_node *aga_bfs_explore(struct aga_graph *g, struct aga_node *n);

/**
 * aga_bfs - Breadth-first search
 * @_n: pointer to current node (output)
 * @_g: graph to search
 * @_start: node to start from
 *
 * Performs a breadth first search.  The block following this macro is
 * executed with @_n set first to @_start, then to each node reachable
 * from @_start in depth first search order.
 *
 * aga_bfs_start() must be called before this macro is used.
 */
#define aga_bfs(_n, _g, _start)					\
	for ((_n) = (_start) ; ((_n) = aga_bfs_explore((_g), (_n))) != NULL; )

/*
 * Dijkstra's algorithm
 */

/**
 * aga_dijkstra_start - Start Dijkstra's algorithm
 * @g: graph
 * @source: source node
 *
 * Start's Dijkstra's algorithm on @g to find shortest paths from node
 * @source, to other nodes in @g.
 */
int aga_dijkstra_start(struct aga_graph *g, struct aga_node *source);


/**
 * aga_dijkstra_step - Find the node with the next shortest path
 * @g: graph
 *
 * The first call to this function returns the source node specified
 * in aga_dijkstra_start().  Subsequent calls return the next closest
 * node to source by shortest path cost.  Returns NULL if no further
 * nodes are reachable from source.
 */
struct aga_node *aga_dijkstra_step(struct aga_graph *g);

/**
 * aga_dijkstra_path - Find the shortest path to a node
 * @g: graph
 * @dest: destination node
 * @prev: Second last node in the path *output)
 * @prevedge: Last edge in the path
 *
 * Finds the shortest path from the source node (specified in
 * aga_dijkstra_start() to @dest using Dijkstra's algorithm.
 *
 * If no path exists, return false.
 *
 * If a path does exist, returns true.  Additionally if @total_cost is
 * non-NULL, store the total cost of the path in *@total_cost, if
 * @prev is non-NULL, store the node in the path immediately before
 * @dest in *@prev and if @prevedge is non-NULL stores the edge which
 * leads from *@prev to @dest in *@prevedge.
 *
 * If @dest is the same as source, 0 will be stored in @cost, and NULL
 * will be stored in *@prev and *@prevedge.
 *
 * The full path from source to @dest can be determined by repeatedly
 * calling aga_dijkstra_path() on *@prev.
 *
 * NOTE: Dijkstra's algorithm will not work correctly on a graph which
 * has negative costs on some edges.  If aga detects this case, it
 * will set aga_error() to AGA_ERR_NEGATIVE_COST.  However,
 * aga_dijkstra_path() may produce incorrect results without detecting
 * this situation.  aga_dijkstra_all_paths() *is* guaranteed to
 * discover any negative cost edge reachable from the starting node.
 */
bool aga_dijkstra_path(struct aga_graph *g, struct aga_node *dest,
		       aga_icost_t *total_cost,
		       struct aga_node **prev, const void **prevedge);

/**
 * aga_dijkstra_all_paths - Find shortest paths to all reachable nodes
 * @g: graph
 *
 * Finds shortest paths from the source node (specified in
 * aga_dijkstra_start()) to all other reachable nodes in @g.  No
 * results are returned directly, but between calling
 * aga_dijkstra_all_paths() and aga_finish, aga_dijkstra_path() is
 * guaranteed to complete in O(1) time for all destinations.
 */
void aga_dijkstra_all_paths(struct aga_graph *g);

#endif /* CCAN_AGA_H */
