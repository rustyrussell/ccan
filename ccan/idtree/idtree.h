#ifndef CCAN_IDTREE_H
#define CCAN_IDTREE_H
#include <stdbool.h>

/**
 * idtree_new - create an idr_context
 * @mem_ctx: talloc parent to allocate from (may be NULL).
 *
 * Allocate an empty id tree.  You can free it with talloc_free().
 */
struct idtree *idtree_new(void *mem_ctx);

/**
 * idtree_add - get lowest available id, and assign a pointer to it.
 * @idtree: the tree to allocate from
 * @ptr: the non-NULL pointer to associate with the id
 * @limit: the maximum id to allocate (ie. INT_MAX means no limit).
 *
 * This returns a non-negative id number, or -1 if all are taken.
 */
int idtree_add(struct idtree *idtree, const void *ptr, int limit);

/**
 * idtree_add_above - get lowest available id, starting at a given value.
 * @idtree: the tree to allocate from
 * @ptr: the non-NULL pointer to associate with the id
 * @starting_id: the minimum id value to consider.
 * @limit: the maximum id to allocate (ie. INT_MAX means no limit).
 *
 * This returns a non-negative id number, or -1 if all are taken.
 */
int idtree_add_above(struct idtree *idtree, const void *ptr,
		     int starting_id, int limit);

/**
 * idtree_lookup - look up a given id
 * @idtree: the tree to look in
 * @id: the id to look up
 *
 * Returns NULL if the value is not found, otherwise the pointer value
 * set with the idtree_add()/idtree_add_above().
 */
void *idtree_lookup(const struct idtree *idtree, int id);

/**
 * idtree_remove - remove a given id.
 * @idtree: the tree to remove from
 * @id: the id to remove.
 *
 * Returns false if the id was not in the tree.
 */
bool idtree_remove(struct idtree *idtree, int id);
#endif /* CCAN_IDTREE_H */
