/* Licensed under GPLv2+ - see LICENSE file for details */
#ifndef CCAN_IDTREE_H
#define CCAN_IDTREE_H
#include <stdbool.h>

/**
 * idtree_new - create an idr_context
 * @mem_ctx: talloc parent to allocate from (may be NULL).
 *
 * Allocate an empty id tree.  You can free it with talloc_free().
 *
 * Example:
 *	static struct idtree *ids;
 *
 *	static void init(void)
 *	{
 *		ids = idtree_new(NULL);
 *		if (!ids)
 *			err(1, "Failed to allocate idtree");
 *	}
 */
struct idtree *idtree_new(void *mem_ctx);

/**
 * idtree_add - get lowest available id, and assign a pointer to it.
 * @idtree: the tree to allocate from
 * @ptr: the non-NULL pointer to associate with the id
 * @limit: the maximum id to allocate (ie. INT_MAX means no limit).
 *
 * This returns a non-negative id number, or -1 if all are taken.
 *
 * Example:
 *	struct foo {
 *		unsigned int id;
 *		// ...
 *	};
 *
 *	// Create a new foo, assigning an id.
 *	static struct foo *new_foo(void)
 *	{
 *		int id;
 *		struct foo *foo = malloc(sizeof(*foo));
 *		if (!foo)
 *			return NULL;
 *
 *		id = idtree_add(ids, foo, INT_MAX);
 *		if (id < 0) {
 *			free(foo);
 *			return NULL;
 *		}
 *		foo->id = id;
 *		return foo;
 *	}
 */
int idtree_add(struct idtree *idtree, const void *ptr, int limit);

/**
 * idtree_add_above - get lowest available id, starting at a given value.
 * @idtree: the tree to allocate from
 * @ptr: the non-NULL pointer to associate with the id
 * @starting_id: the minimum id value to consider.
 * @limit: the maximum id to allocate (ie. INT_MAX means no limit).
 *
 * Example:
 *	static int last_id = -1;
 *
 *	// Create a new foo, assigning a consecutive id.
 *	// This maximizes the time before ids roll.
 *	static struct foo *new_foo_inc_id(void)
 *	{
 *		int id;
 *		struct foo *foo = malloc(sizeof(*foo));
 *		if (!foo)
 *			return NULL;
 *
 *		id = idtree_add_above(ids, foo, last_id+1, INT_MAX);
 *		if (id < 0) {
 *			id = idtree_add(ids, foo, INT_MAX);
 *			if (id < 0) {
 *				free(foo);
 *				return NULL;
 *			}
 *		}
 *		last_id = id;
 *		foo->id = id;
 *		return foo;
 *	}
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
 *
 * Example:
 *	// Look up a foo for a given ID.
 *	static struct foo *find_foo(unsigned int id)
 *	{
 *		return idtree_lookup(ids, id);
 *	}
 */
void *idtree_lookup(const struct idtree *idtree, int id);

/**
 * idtree_remove - remove a given id.
 * @idtree: the tree to remove from
 * @id: the id to remove.
 *
 * Returns false if the id was not in the tree.
 *
 * Example:
 *	// Look up a foo for a given ID.
 *	static void free_foo(struct foo *foo)
 *	{
 *		bool exists = idtree_remove(ids, foo->id);
 *		assert(exists);
 *		free(foo);
 *	}
 */
bool idtree_remove(struct idtree *idtree, int id);
#endif /* CCAN_IDTREE_H */
