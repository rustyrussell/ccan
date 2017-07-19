/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <stdbool.h>

#include <ccan/aga/aga.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable.h>
#include <ccan/container_of/container_of.h>
#include <ccan/tal/tal.h>

#include <ccan/agar/agar.h>

#define HASH_BASE 0

struct agar_node {
	const void *nr;
	struct aga_node n;
};

struct agar_state {
	struct agar_graph *gr;
	struct aga_graph g;
	struct htable nodes;
};

static size_t agar_node_hash(const struct agar_node *nn)
{
	return hash_pointer(nn->nr, HASH_BASE);
}

static size_t agar_rehash(const void *elem, void *p)
{
	return agar_node_hash(elem);
}

static bool agar_node_cmp(const void *candidate, void *ptr)
{
	struct agar_node *nn = (struct agar_node *)candidate;

	return (nn->nr == ptr);
}

static struct aga_node *nr_to_n(struct agar_state *sr, const void *nr)
{
	struct agar_node *nn;
	size_t hash = hash_pointer(nr, HASH_BASE);
	bool rc;

	nn = htable_get(&sr->nodes, hash, agar_node_cmp, nr);
	if (!nn) {
		nn = tal(sr, struct agar_node);
		assert(nn);

		nn->nr = nr;
		aga_node_init(&nn->n);

		rc = htable_add(&sr->nodes, hash, nn);
		assert(rc);
	}

	return &nn->n;
}

static const void *n_to_nr(struct agar_state *sr, const struct aga_node *n)
{
	struct agar_node *nn = container_of(n, struct agar_node, n);

	return nn->nr;
}

static int convert_edge_info(const struct aga_graph *g,
			     const struct aga_node *n,
			     const void *e, struct aga_edge_info *ei)
{
	struct agar_state *sr = container_of(g, struct agar_state, g);
	const void *nr = n_to_nr(sr, n);
	struct agar_edge_info eir;
	int rc;

	eir.to = NULL;
	eir.icost = ei->icost; /* Inherit the default from aga */

	rc = sr->gr->edge_info(sr->gr, nr, e, &eir);

	if (eir.to)
		ei->to = nr_to_n(sr, eir.to);
	else
		ei->to = NULL;

	ei->icost = eir.icost;

	return rc;
}

static const void *convert_first_edge(const struct aga_graph *g,
				      const struct aga_node *n)
{
	struct agar_state *sr = container_of(g, struct agar_state, g);
	const void *nr = n_to_nr(sr, n);

	return sr->gr->first_edge(sr->gr, nr);
}

static const void *convert_next_edge(const struct aga_graph *g,
				     const struct aga_node *n,
				     const void *e)
{
	struct agar_state *sr = container_of(g, struct agar_state, g);
	const void *nr = n_to_nr(sr, n);

	return sr->gr->next_edge(sr->gr, nr, e);
}

void agar_init_graph(struct agar_graph *gr,
		     agar_first_edge_fn first_edge,
		     agar_next_edge_fn next_edge,
		     agar_edge_info_fn edge_info)
{
	gr->edge_info = edge_info;
	gr->first_edge = first_edge;
	gr->next_edge = next_edge;
}

int agar_error(struct agar_state *sr)
{
	return aga_error(&sr->g);
}

static void agar_destruct_htable(struct agar_state *sr)
{
	htable_clear(&sr->nodes);
}

static struct agar_state *agar_new(void *ctx, struct agar_graph *gr)
{
	struct agar_state *sr = tal(ctx, struct agar_state);

	assert(sr);

	sr->gr = gr;
	htable_init(&sr->nodes, agar_rehash, NULL);
	tal_add_destructor(sr, agar_destruct_htable);
	aga_init_graph(&sr->g, convert_first_edge, convert_next_edge,
		       convert_edge_info);

	return sr;
}

const void *agar_first_edge(const struct agar_graph *gr, const void *nr)
{
	return gr->first_edge(gr, nr);
}

const void *agar_next_edge(const struct agar_graph *gr,
			   const void *nr, const void *e)
{
	if (!e)
		return NULL;
	else
		return gr->next_edge(gr, nr, e);
}

int agar_edge_info(const struct agar_graph *gr, const void *nr, const void *e,
		   struct agar_edge_info *eir)
{
	int rc;

	eir->to = NULL;
	eir->icost = 1;
	rc = gr->edge_info(gr, nr, e, eir);
	assert(rc <= 0);
	return rc;
}

/*
 * Depth first search
 */

struct agar_dfs_state {
	struct agar_state sr;
};

struct agar_state *agar_dfs_new(void *ctx, struct agar_graph *gr)
{
	struct agar_state *sr = agar_new(ctx, gr);

	if (aga_dfs_start(&sr->g) < 0) {
		tal_free(sr);
		return NULL;
	}

	return sr;
}

bool agar_dfs_explore(struct agar_state *sr, const void *nr,
		      const void **nextr)
{
	struct aga_node *next;

	next = aga_dfs_explore(&sr->g, nr_to_n(sr, nr));
	if (!next)
		return false;

	*nextr = n_to_nr(sr, next);
	return true;
}

/*
 * Breadth first search
 */

struct agar_state *agar_bfs_new(void *ctx, struct agar_graph *gr)
{
	struct agar_state *sr = agar_new(ctx, gr);

	if (aga_bfs_start(&sr->g) < 0) {
		tal_free(sr);
		return NULL;
	}

	return sr;
}

bool agar_bfs_explore(struct agar_state *sr, const void *nr,
		      const void **nextr)
{
	struct aga_node *next;

	next = aga_bfs_explore(&sr->g, nr_to_n(sr, nr));
	if (!next)
		return false;

	*nextr = n_to_nr(sr, next);
	return true;
}

/*
 * Dijkstra's algorithm
 */

struct agar_state *agar_dijkstra_new(void *ctx, struct agar_graph *gr,
				     const void *nr)
{
	struct agar_state *sr = agar_new(ctx, gr);

	if (aga_dijkstra_start(&sr->g, nr_to_n(sr, nr)) < 0) {
		tal_free(sr);
		return NULL;
	}

	return sr;
}

bool agar_dijkstra_step(struct agar_state *sr, const void **nextr)
{
	struct aga_node *next = aga_dijkstra_step(&sr->g);

	if (!next)
		return false;

	*nextr = n_to_nr(sr, next);
	return true;
}

bool agar_dijkstra_path(struct agar_state *sr, const void *destr,
			aga_icost_t *total_cost,
			const void **prevr, const void **prevedge)
{
	struct aga_node *dest = nr_to_n(sr, destr);
	struct aga_node *prev;

	if (!aga_dijkstra_path(&sr->g, dest, total_cost, &prev, prevedge))
		return false;

	/*
	 * When destr is the same as the source node, there obviously
	 * isn't a previous node or edge.  In that case aga, sets them
	 * to NULL.  But for agar, NULL could be a valid node
	 * references (particularly if using ptrint).  So we don't
	 * have much choice here but to leave *prevr as undefined when
	 * destr is the source node. */
	if (prevr && prev)
		*prevr = n_to_nr(sr, prev);

	return true;
}

void agar_dijkstra_complete(struct agar_state *sr)
{
	aga_dijkstra_complete(&sr->g);
}


/*
 * Bellman-Ford algorithm
 */

struct agar_state *agar_bellman_ford_new(void *ctx, struct agar_graph *gr,
					 const void *nr)
{
	struct agar_state *sr = agar_new(ctx, gr);

	if (aga_bellman_ford_start(&sr->g, nr_to_n(sr, nr)) < 0) {
		tal_free(sr);
		return NULL;
	}

	return sr;
}

bool agar_bellman_ford_path(struct agar_state *sr, const void *destr,
			    aga_icost_t *total_cost,
			    const void **prevr, const void **prevedge)
{
	struct aga_node *dest = nr_to_n(sr, destr);
	struct aga_node *prev;

	if (!aga_bellman_ford_path(&sr->g, dest, total_cost, &prev, prevedge))
		return false;

	/*
	 * When destr is the same as the source node, there obviously
	 * isn't a previous node or edge.  In that case aga sets them
	 * to NULL.  But for agar, NULL could be a valid node
	 * references (particularly if using ptrint).  So we don't
	 * have much choice here but to leave *prevr as undefined when
	 * destr is the source node. */
	if (prevr && prev)
		*prevr = n_to_nr(sr, prev);

	return true;
}

void agar_bellman_ford_complete(struct agar_state *sr)
{
	aga_bellman_ford_complete(&sr->g);
}
