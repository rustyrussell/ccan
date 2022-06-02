/* MIT (BSD) license - see LICENSE file for details */
#ifndef CCAN_UNGRAPH_H
#define CCAN_UNGRAPH_H
#include <ccan/tal/tal.h>
#include <ccan/typesafe_cb/typesafe_cb.h>

/**
 * ungraph: extract a graph from an ASCII graph.
 * @ctx: context for callbacks, and/or returned errstr.
 * @str: a string containing a graph.
 * @add_node: callback for a new node, returns node.
 * @add_edge: callback for a new edge, with tal_count(labels).
 * @arg: callback argument.
 *
 * On success, returns NULL.  On failure, returns some error message
 * (allocated off @ctx, or returned from callbacks).
 *
 * If @add_node returns NULL, it must set @errstr. @add_edge
 * returns the error message directly.
 *
 * @add_node and @add_edge can tal_steal the name/labels if they want,
 * otherwise they will be freed.
 */
const char *ungraph_(const tal_t *ctx,
		     const char *str,
		     void *(*add_node)(const tal_t *ctx,
				       const char *name,
				       const char **errstr,
				       void *arg),
		     const char *(*add_edge)(const tal_t *ctx,
					     void *source_node,
					     void *dest_node,
					     bool bidir,
					     const char **labels,
					     void *arg),
		     void *arg);

#define ungraph(ctx, str, add_node, add_edge, arg) 			\
	ungraph_((ctx), (str),						\
		 typesafe_cb_preargs(void *, void *,			\
				     (add_node), (arg),			\
				     const tal_t *,			\
				     const char *,			\
				     const char **errstr),		\
		 typesafe_cb_preargs(const char *, void *,		\
				     (add_edge), (arg),			\
				     const tal_t *,			\
				     void *,				\
				     void *,				\
				     bool,				\
				     const char **),			\
		 arg)
#endif /* CCAN_UNGRAPH_H */
