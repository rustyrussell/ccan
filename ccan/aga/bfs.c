/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <ccan/aga/aga.h>
#include "private.h"

/*
 * Breadth first search
 */

typedef LQUEUE(struct aga_node, u.bfs.next) bfs_queue;

static bool bfs_enqueue(struct aga_graph *g, bfs_queue *queue,
			struct aga_node *n)
{
	if (!aga_update_node(g, n))
		return false;

	lqueue_enqueue(queue, n);
	n->u.bfs.edge = aga_first_edge(g, n);
	return true;
}

static struct aga_node *bfs_front(bfs_queue *queue)
{
	return lqueue_front(queue);
}

static void bfs_dequeue(bfs_queue *queue)
{
	(void) lqueue_dequeue(queue);
}

int aga_bfs_start(struct aga_graph *g)
{
	int rc;

	rc = aga_start(g);
	if (rc < 0)
		return rc;

	return 0;
}

struct aga_node *aga_bfs_explore(struct aga_graph *g, struct aga_node *n)
{
	bfs_queue queue = LQUEUE_INIT;

	if (!aga_check_state(g))
		return NULL;

	if (!n)
		return NULL;
 
	if (bfs_enqueue(g, &queue, n))
		return n;

	lqueue_init_from_back(&queue, n);

	while ((n = bfs_front(&queue))) {
		const void *e = n->u.bfs.edge;
		int err;
		struct aga_edge_info ei;

		if (!e) {
			/* out of edges, back up */
			bfs_dequeue(&queue);
			continue;
		}

		n->u.bfs.edge = aga_next_edge(g, n, e);

		err = aga_edge_info(g, n, e, &ei);
		if (err < 0) {
			aga_fail(g, err);
			return NULL;
		}
		if (!ei.to) {
			/* missing edge */
			continue;
		}

		if (!bfs_enqueue(g, &queue, ei.to)) {
			/* already visited node */
			continue;
		}

		return ei.to;
	}
	
	return NULL;
}
