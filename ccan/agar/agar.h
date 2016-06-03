/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_AGAR_H
#define CCAN_AGAR_H
#include "config.h"
#include <string.h>
#include <stdbool.h>

#include <ccan/aga/aga.h>

struct agar_edge_info {
	const void *to;
	aga_icost_t icost; /* integer edge cost */
};

struct agar_graph;
struct agar_state;

typedef int (*agar_edge_info_fn)(const struct agar_graph *gr,
				 const void *nr,
				 const void *e,
				 struct agar_edge_info *ei);
typedef const void *(*agar_first_edge_fn)(const struct agar_graph *gr,
					  const void *nr);
typedef const void *(*agar_next_edge_fn)(const struct agar_graph *gr,
					 const void *nr,
					 const void *e);

struct agar_graph {
	agar_edge_info_fn edge_info;
	agar_first_edge_fn first_edge;
	agar_next_edge_fn next_edge;
};

#define AGAR_INIT_GRAPH(fe, ne, ei) \
	{ (ei), (fe), (ne), }

void agar_init_graph(struct agar_graph *gr,
		     agar_first_edge_fn first_edge,
		     agar_next_edge_fn next_edge,
		     agar_edge_info_fn edge_info);
int agar_error(struct agar_state *sr);

const void *agar_first_edge(const struct agar_graph *g, const void *nr);
const void *agar_next_edge(const struct agar_graph *g, const void *nr,
			   const void *e);
int agar_edge_info(const struct agar_graph *g, const void *nr, const void *e,
		   struct agar_edge_info *eir);

#define agar_for_each_edge(_e, _gr, _nr)				\
	for ((_e) = agar_first_edge((_gr), (_nr)); (_e);		\
	     (_e) = aga_next_edge((_gr), (_nr), (_e)))

#define agar_for_each_edge_info(_e, _eir, _err, _gr, _nr)		\
	for ((_e) = agar_first_edge((_gr), (_nr));			\
	     (_e) && ((((_err) = agar_edge_info((_gr), (_nr), (_e), &(_eir)))) == 0); \
	     (_e) = agar_next_edge((_gr), (_nr), (_e)))			\
		if ((_eir).to)

/*
 * Depth first search
 */
struct agar_state *agar_dfs_new(void *ctx, struct agar_graph *gr);
bool agar_dfs_explore(struct agar_state *sr, const void *nr,
		      const void **nextr);
#define agar_dfs(_nr, _sr, _startr)					\
	for ((_nr) = (_startr); agar_dfs_explore((_sr), (_nr), &(_nr)); )

/*
 * Breadth first search
 */
struct agar_state *agar_bfs_new(void *ctx, struct agar_graph *gr);
bool agar_bfs_explore(struct agar_state *sr, const void *nr,
		      const void **nextr);
#define agar_bfs(_nr, _sr, _startr)					\
	for ((_nr) = (_startr); agar_bfs_explore((_sr), (_nr), &(_nr)); )

/*
 * Dijkstra's algorithm
 */

struct agar_state *agar_dijkstra_new(void *ctx, struct agar_graph *gr,
				     const void *nr);
bool agar_dijkstra_step(struct agar_state *sr, const void **nextr);
bool agar_dijkstra_path(struct agar_state *sr, const void *destr,
			aga_icost_t *total_cost,
			const void **prevr, const void **prevedge);
void agar_dijkstra_all_paths(struct agar_state *sr);

#endif /* CCAN_AGAR_H */
