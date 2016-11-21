/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/check_type/check_type.h>
#include <ccan/order/order.h>

#include <ccan/aga/aga.h>
#include "private.h"

/*
 * The Bellman-Ford algorithm
 */

static bool candidate_path(struct aga_graph *g, struct aga_node *node,
			   aga_icost_t distance,
			   struct aga_node *prev, const void *prevedge)
{
	if (aga_update_node(g, node)) {
		/* New node, treat as having infinite distance */
		node->u.bellman_ford.distance = distance;
		node->u.bellman_ford.prev = prev;
		node->u.bellman_ford.prevedge = prevedge;

		node->u.bellman_ford.list = g->state.bellman_ford.nodelist;
		g->state.bellman_ford.nodelist = node;
		g->state.bellman_ford.nnodes++;

		return true;
	} else if (distance < node->u.bellman_ford.distance) {
		node->u.bellman_ford.distance = distance;
		node->u.bellman_ford.prev = prev;
		node->u.bellman_ford.prevedge = prevedge;
	}
	return false;
}

int aga_bellman_ford_start(struct aga_graph *g, struct aga_node *source)
{
	int rc;

	/* Make sure we're actually using the right ordering for
	 * aga_icost_t */
	BUILD_ASSERT(check_types_match(long, aga_icost_t) == 0);

	rc = aga_start(g);
	if (rc < 0)
		return rc;

	g->state.bellman_ford.nodelist = NULL;
	g->state.bellman_ford.nnodes = 0;
	g->state.bellman_ford.npasses = 0;

	candidate_path(g, source, 0, NULL, NULL);

	return 0;
}

static bool aga_bellman_ford_step(struct aga_graph *g)
{
	struct aga_node *n;
	const void *e;
	struct aga_edge_info ei;
	int err;
	bool newnode = false;

	if (!aga_check_state(g))
		return false;

	for (n = g->state.bellman_ford.nodelist;
	     n; n = n->u.bellman_ford.list) {
		aga_for_each_edge_info(e, ei, err, g, n) {
			aga_icost_t dist = n->u.bellman_ford.distance
				+ ei.icost;
			newnode = newnode || candidate_path(g, ei.to, dist, n, e);
		}
		if (err) {
			aga_fail(g, err);
			return false;
		}
	}
	g->state.bellman_ford.npasses++;
	return newnode || (g->state.bellman_ford.npasses
			   < g->state.bellman_ford.nnodes);
}

void aga_bellman_ford_complete(struct aga_graph *g)
{
	struct aga_node *n;
	const void *e;
	struct aga_edge_info ei;
	int err;

	if (!aga_check_state(g))
		return;

	while (aga_bellman_ford_step(g))
		;

	/* Check for negative cycles */
	for (n = g->state.bellman_ford.nodelist;
	     n; n = n->u.bellman_ford.list) {
		aga_for_each_edge_info(e, ei, err, g, n) {
			if ((n->u.bellman_ford.distance + ei.icost)
			    < ei.to->u.bellman_ford.distance) {
				aga_fail(g, AGA_ERR_NEGATIVE_COST);
				return;
			}
		}
		if (err) {
			aga_fail(g, err);
			return;
		}
	}
}

bool aga_bellman_ford_path(struct aga_graph *g, struct aga_node *node,
			   aga_icost_t *total_cost,
			   struct aga_node **prev, const void **prevedge)
{
	aga_bellman_ford_complete(g);

	if (!aga_check_state(g))
		return false;

	if (aga_node_needs_update(g, node))
		return false;

	if (total_cost)
		*total_cost = node->u.bellman_ford.distance;
	if (prev)
		*prev = node->u.bellman_ford.prev;
	if (prevedge)
		*prevedge = node->u.bellman_ford.prevedge;

	return true;
}
