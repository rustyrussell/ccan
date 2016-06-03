#ifndef _SIMPLE_GRAPHR_H
#define _SIMPLE_GRAPHR_H

#include <stdbool.h>

#define MAX_EDGES	16 /* Max edges per node */

struct adjacency_listr {
	int from;
	int to[MAX_EDGES];
};

/* Trivial graph
 *
 *	(A)
 *
 * The simplest possible graph: one node, no edges
 */
struct trivial_graphr {
	struct agar_graph gr;
};
extern struct trivial_graphr trivial_graphr;
static const struct adjacency_listr trivial_adjacencyr[] = {
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
struct parallel_graphr {
	int nlinks;
	int cheaplink;
	struct agar_graph gr;
};
void parallel_graphr_init(struct parallel_graphr *pgr, int nlinks,
			  int cheaplink);
static const struct adjacency_listr parallel_adjacencyr_nlinks3[] = {
	{1, {2, 2, 2}},
	{2, {}},
	{},
};

/* Full graph
 *
 * n nodes with an edge from every node to every other node (including
 * itself)
 */
struct full_graphr {
	int nnodes;
	struct agar_graph gr;
};
void full_graphr_init(struct full_graphr *fgr, int nnodes);
static const struct adjacency_listr full_adjacencyr_5[] = {
	{1, {1, 2, 3, 4, 5}},
	{2, {1, 2, 3, 4, 5}},
	{3, {1, 2, 3, 4, 5}},
	{4, {1, 2, 3, 4, 5}},
	{5, {1, 2, 3, 4, 5}},
	{},
};
const void *full_first_edge_r(const struct agar_graph *gr, const void *nr);
const void *full_next_edge_r(const struct agar_graph *gr,
			     const void *nr, const void *e);


/* Chain graph
 *
 *  --> --> -->
 * A   B   C   D
 *  <-- <-- <--
 *
 * nnodes nodes arranged in a linear sequence, edges from each node to
 * the previous and next
 */
struct chain_graphr {
	struct full_graphr fgr;
};
void chain_graphr_init(struct chain_graphr *cgr, int nnodes);
static const struct adjacency_listr chain_adjacencyr_8[] = {
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
struct grid_graphr {
	int nx, ny;
	bool right, down, left, up;
	struct agar_graph gr;
};
void grid_graphr_init(struct grid_graphr *ggr, int nx, int ny,
		     bool right, bool down, bool left, bool up);
static const struct adjacency_listr grid_adjacencyr_3x3_rightdown[] = {
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
static const struct adjacency_listr grid_adjacencyr_3x3_all[] = {
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
struct error_graphr {
	struct agar_graph gr;
};
extern struct error_graphr error_graphr;
static const struct adjacency_listr error_adjacencyr[] = {
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
struct traversal1_graphr {
	struct agar_graph gr;
};
extern struct traversal1_graphr traversal1_graphr;
static const struct adjacency_listr traversal1_adjacency[] = {
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
struct shortcut1_graphr {
	struct agar_graph gr;
};
extern struct shortcut1_graphr shortcut1_graphr;
static const struct adjacency_listr shortcut1_adjacencyr[] = {
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
struct shortcut2_graphr {
	struct agar_graph gr;
};
extern struct shortcut2_graphr shortcut2_graphr;
static const struct adjacency_listr shortcut2_adjacencyr[] = {
	{1, {3, 2}},
	{2, {3}},
	{3, {}},
	{},
};

#endif /* _SIMPLE_GRAPHR_H */
