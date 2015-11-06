#ifndef _TEST_GRAPHS_H
#define _TEST_GRAPHS_H

#include <stdbool.h>

#define MAX_NODES	16
#define MAX_EDGES	16 /* Max edges per node */

struct simple_graph {
	struct aga_graph g;
	/* We don't use nodes[0] just to avoid awkward -1 and +1 in
	 * the code */
	struct aga_node nodes[MAX_NODES + 1];
};

void simple_graph_init_(struct simple_graph *sg);
#define simple_graph_init(sg_, fefn_, nefn_, eifn_)			\
	do {								\
		simple_graph_init_((sg_));				\
		aga_init_graph(&(sg_)->g, (fefn_), (nefn_), (eifn_));	\
	} while (0)

struct adjacency_list {
	int from;
	int to[MAX_EDGES];
};

/* Trivial graph
 *
 *	(A)
 *
 * The simplest possible graph: one node, no edges
 */
struct trivial_graph {
	struct simple_graph sg;
};
void trivial_graph_init(struct trivial_graph *tg);
static const struct adjacency_list trivial_adjacency[] = {
	{1, {}},
	{},
};

/* Parallel graph
 *
 *          --
 *         /  \
 *      (A)    => (B)
 *         \  /
 *          --
 *
 * Two nodes (A & B), with PARALLEL_NLINKS edges all from A->B.
 */
struct parallel_graph {
	int nlinks;
	int cheaplink;
	struct simple_graph sg;
};
void parallel_graph_init(struct parallel_graph *pg, int nlinks, int cheaplink);
static const struct adjacency_list parallel_adjacency_nlinks3[] = {
	{1, {2, 2, 2}},
	{2, {}},
	{},
};

/* Full graph
 *
 * n nodes with an edge from every node to every other node (including
 * itself)
 */
struct full_graph {
	int nnodes;
	struct simple_graph sg;
};
void full_graph_init(struct full_graph *fg, int nnodes);
static const struct adjacency_list full_adjacency_5[] = {
	{1, {1, 2, 3, 4, 5}},
	{2, {1, 2, 3, 4, 5}},
	{3, {1, 2, 3, 4, 5}},
	{4, {1, 2, 3, 4, 5}},
	{5, {1, 2, 3, 4, 5}},
	{},
};
struct aga_node *full_first_edge(const struct aga_graph *g,
				 const struct aga_node *node);
struct aga_node *full_next_edge(const struct aga_graph *g,
				const struct aga_node *node,
				struct aga_node *edge);


/* Chain graph
 *
 *  --> --> -->
 * A   B   C   D
 *  <-- <-- <--
 *
 * nnodes nodes arranged in a linear sequence, edges from each node to
 * the previous and next
 */
struct chain_graph {
	struct full_graph fg;
};
void chain_graph_init(struct chain_graph *cg, int nnodes);
static const struct adjacency_list chain_adjacency_8[] = {
	{1, {2}},
	{2, {1, 3}},
	{3, {2, 4}},
	{4, {3, 5}},
	{5, {4, 6}},
	{6, {5, 7}},
	{7, {6, 8}},
	{8, {7}},
	{},
};


/* Grid graph(s)
 *
 * A -> B -> C
 * |    |    |
 * v    v    v
 * D -> E -> F
 * |    |    |
 * v    v    v
 * G -> H -> I
 *
 * nx * ny nodes arranged in an nx * ny grid.  Depending on
 * parameters, edges to the node to the right / down / left / up of
 * each node
 */
struct grid_graph {
	int nx, ny;
	bool right, down, left, up;
	struct simple_graph sg;
};
void grid_graph_init(struct grid_graph *gg, int nx, int ny,
		     bool right, bool down, bool left, bool up);
static const struct adjacency_list grid_adjacency_3x3_rightdown[] = {
	{1, {2, 4}},
	{2, {3, 5}},
	{3, {6}},
	{4, {5, 7}},
	{5, {6, 8}},
	{6, {9}},
	{7, {8}},
	{8, {9}},
	{9, {}},
	{},
};
static const struct adjacency_list grid_adjacency_3x3_all[] = {
	{1, {2, 4}},
	{2, {3, 5, 1}},
	{3, {6, 2}},
	{4, {5, 7, 1}},
	{5, {6, 8, 4, 2}},
	{6, {9, 5, 3}},
	{7, {8, 4}},
	{8, {9, 7, 5}},
	{9, {8, 6}},
	{},
};

/* Error graph
 *
 * A -> B
 *
 * C -> D -> ???
 *
 * This is for testing reporting of errors by the edge_info function.
 * 5 nodes are arranged as above, with the link from D always
 * returning an error.
 */
struct error_graph {
	struct simple_graph sg;
};
void error_graph_init(struct error_graph *eg);
static const struct adjacency_list error_adjacency[] = {
	{1, {2}},
	{2, {}},
	{3, {4}},
	{4, {-1}},
	{},
};

/* Traversal-1 graph
 *
 *        -> D <-
 *       /       \
 *   -> B         G <-
 *  /    \       /    \
 * A      => E <=      I
 *  \    /       \    /
 *   -> C         H <-
 *       \       /
 *        -> F <-
 *
 * This provides an example of a graph which can't be traversed (with
 * DFS or BFS) from a single starting node.  It can be traversed with
 * two starting points, but several nodes can be reached from either,
 * complicating the logic to avoid double-traversal.
 */
struct traversal1_graph {
	struct simple_graph sg;
};
void traversal1_graph_init(struct traversal1_graph *t1g);
static const struct adjacency_list traversal1_adjacency[] = {
	{1, {2, 3}},
	{2, {4, 5}},
	{3, {5, 6}},
	{4, {}},
	{5, {}},
	{6, {}},
	{7, {5, 4}},
	{8, {6, 5}},
	{9, {8, 7}},
	{},
};

/* Shortcut-1 graph
 *
 *   A ---- (3) -----> C
 *    \             /
 *     (1)-> B  --(1)
 *
 * This provides an example of a graph where the lowest cost path from
 * (A) to (C) is not the path with the smallest number od edges.
 */
struct shortcut1_graph {
	struct simple_graph sg;
};
void shortcut1_graph_init(struct shortcut1_graph *s1g);
static const struct adjacency_list shortcut1_adjacency[] = {
	{1, {3, 2}},
	{2, {3}},
	{3, {}},
	{},
};

/* Shortcut-2 graph
 *
 *   A ---- (2) -----> C
 *    \             /
 *     (2)-> B  --(-1)
 *
 * This provides an example of a graph with a negative edge cost, but
 * no negative cost cycles (and so still with well defined shortest
 * paths).
 */
struct shortcut2_graph {
	struct simple_graph sg;
};
void shortcut2_graph_init(struct shortcut2_graph *s2g);
static const struct adjacency_list shortcut2_adjacency[] = {
	{1, {3, 2}},
	{2, {3}},
	{3, {}},
	{},
};

#endif /* _TEST_GRAPHS_H */
