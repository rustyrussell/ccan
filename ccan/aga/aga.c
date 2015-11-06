/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <assert.h>

#include <ccan/aga/aga.h>

#include "private.h"

void aga_init_graph_(struct aga_graph *g,
		     aga_first_edge_fn first_edge,
		     aga_next_edge_fn next_edge,
		     aga_edge_info_fn edge_info)
{
	g->sequence = 0;
	g->error = AGA_ERR_NONE;

	g->first_edge = first_edge;
	g->next_edge = next_edge;
	g->edge_info = edge_info;
}

int aga_error(const struct aga_graph *g)
{
	return g->error;
}

void aga_fail(struct aga_graph *g, int error)
{
	g->error = error;
}

int aga_start(struct aga_graph *g)
{
	if (g->sequence & 1) /* Odd means someone's already running */
		return -1;
	assert(g->error == 0);
	/* FIXME: Want an atomic cmpxchg to make this thread safe */
	++g->sequence;
	return 0;
}

bool aga_check_state(const struct aga_graph *g)
{
	if (!(g->sequence & 1))
		return false; /* No algo in progress */
	if (g->error)
		return false; /* error state */
	return true;
}

void aga_finish(struct aga_graph *g)
{
	assert(g->sequence & 1);
	g->error = AGA_ERR_NONE;
	g->sequence++;
}

bool aga_node_needs_update(const struct aga_graph *g,
			   const struct aga_node *node)
{
	return (node->sequence != g->sequence);
}

bool aga_update_node(const struct aga_graph *g, struct aga_node *node)
{
	if (!aga_node_needs_update(g, node))
		return false;

	node->sequence = g->sequence;
	return true;
}

const void *aga_first_edge(const struct aga_graph *g, const struct aga_node *n)
{
	return g->first_edge(g, n);
}

const void *aga_next_edge(const struct aga_graph *g, const struct aga_node *n,
			  const void *e)
{
	if (!e)
		return NULL;
	else
		return g->next_edge(g, n, e);
}

int aga_edge_info(const struct aga_graph *g, const struct aga_node *n,
		  const void *e, struct aga_edge_info *ei)
{
	int rc;

	ei->to = NULL;
	ei->icost = 1;
	rc = g->edge_info(g, n, e, ei);
	assert(rc <= 0);
	return rc;
}
