/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/check_type/check_type.h>
#include <ccan/order/order.h>
#include <ccan/lpq/lpq.h>

#include <ccan/aga/aga.h>
#include "private.h"

/*
 * Dijkstra's algorithm
 */

static void candidate_path(struct aga_graph *g, struct aga_node *node,
			   aga_icost_t distance,
			   struct aga_node *prev, const void *prevedge)
{
	if (aga_update_node(g, node)) {
		/* New node, treat has having infinite distance */
		node->u.dijkstra.distance = distance;
		node->u.dijkstra.prev = prev;
		node->u.dijkstra.prevedge = prevedge;
		node->u.dijkstra.complete = false;

		lpq_enqueue(&g->state.dijkstra, node);
	} else if (distance < node->u.dijkstra.distance) {
		assert(!node->u.dijkstra.complete);

		node->u.dijkstra.distance = distance;
		node->u.dijkstra.prev = prev;
		node->u.dijkstra.prevedge = prevedge;

		lpq_reorder(&g->state.dijkstra, node);
	}
}

int aga_dijkstra_start(struct aga_graph *g, struct aga_node *source)
{
	total_order_by_field(order, long_reverse,
			     struct aga_node, u.dijkstra.distance);
	int rc;

	/* Make sure we're actually using the right ordering for
	 * aga_icost_t */
	BUILD_ASSERT(check_types_match(long, aga_icost_t) == 0);

	rc = aga_start(g);
	if (rc < 0)
		return rc;

	lpq_init(&g->state.dijkstra, order.cb, order.ctx);

	candidate_path(g, source, 0, NULL, NULL);

	return 0;
}

struct aga_node *aga_dijkstra_step(struct aga_graph *g)
{
	struct aga_node *n = lpq_dequeue(&g->state.dijkstra);
	const void *e;
	struct aga_edge_info ei;
	int err;

	if (!aga_check_state(g))
		return NULL;

	if (!n)
		return NULL;

	aga_for_each_edge_info(e, ei, err, g, n) {
		if (ei.icost < 0) {
			aga_fail(g, AGA_ERR_NEGATIVE_COST);
			return NULL;
		}
		candidate_path(g, ei.to,
			       n->u.dijkstra.distance + ei.icost, n, e);
	}
	if (err) {
		aga_fail(g, err);
		return NULL;
	}

	n->u.dijkstra.complete = true;

	return n;
}

bool aga_dijkstra_path(struct aga_graph *g, struct aga_node *node,
		       aga_icost_t *total_cost,
		       struct aga_node **prev, const void **prevedge)
{
	if (!aga_check_state(g))
		return false;

	while (aga_node_needs_update(g, node) || !node->u.dijkstra.complete) {
		if (!aga_dijkstra_step(g))
			return false;
	};

	if (total_cost)
		*total_cost = node->u.dijkstra.distance;
	if (prev)
		*prev = node->u.dijkstra.prev;
	if (prevedge)
		*prevedge = node->u.dijkstra.prevedge;

	return true;
}

void aga_dijkstra_all_paths(struct aga_graph *g)
{
	if (!aga_check_state(g))
		return;

	while (aga_dijkstra_step(g))
		;
}

