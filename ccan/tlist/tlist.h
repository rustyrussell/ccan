/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_TLIST_H
#define CCAN_TLIST_H
#include <ccan/list/list.h>
#include <ccan/tcon/tcon.h>

/**
 * TLIST_TYPE - declare a typed list type (struct tlist)
 * @suffix: the name to use (struct tlist_@suffix)
 * @type: the type the list will contain (void for any type)
 *
 * This declares a structure "struct tlist_@suffix" to use for
 * lists containing this type.  The actual list can be accessed using
 * ".raw" or tlist_raw().
 *
 * Example:
 *	// Defines struct tlist_children
 *	TLIST_TYPE(children, struct child);
 *	struct parent {
 *		const char *name;
 *		struct tlist_children children;
 *		unsigned int num_children;
 *	};
 *
 *	struct child {
 *		const char *name;
 *		struct list_node list;
 *	};
 */
#define TLIST_TYPE(suffix, type)			\
	struct tlist_##suffix {				\
		struct list_head raw;			\
		TCON(type *canary);			\
	}

/**
 * TLIST_INIT - initalizer for an empty tlist
 * @name: the name of the list.
 *
 * Explicit initializer for an empty list.
 *
 * See also:
 *	tlist_init()
 *
 * Example:
 *	static struct tlist_children my_list = TLIST_INIT(my_list);
 */
#define TLIST_INIT(name) { LIST_HEAD_INIT(name.raw) }

/**
 * tlist_check - check head of a list for consistency
 * @h: the tlist_head
 * @abortstr: the location to print on aborting, or NULL.
 *
 * Because list_nodes have redundant information, consistency checking between
 * the back and forward links can be done.  This is useful as a debugging check.
 * If @abortstr is non-NULL, that will be printed in a diagnostic if the list
 * is inconsistent, and the function will abort.
 *
 * Returns non-NULL if the list is consistent, NULL otherwise (it
 * can never return NULL if @abortstr is set).
 *
 * See also: list_check()
 *
 * Example:
 *	static void dump_parent(struct parent *p)
 *	{
 *		struct child *c;
 *
 *		printf("%s (%u children):\n", p->name, p->num_children);
 *		tlist_check(&p->children, "bad child list");
 *		tlist_for_each(&p->children, c, list)
 *			printf(" -> %s\n", c->name);
 *	}
 */
#define tlist_check(h, abortstr) \
	list_check(&(h)->raw, (abortstr))

/**
 * tlist_init - initialize a tlist
 * @h: the tlist to set to the empty list
 *
 * Example:
 *	...
 *	struct parent *parent = malloc(sizeof(*parent));
 *
 *	tlist_init(&parent->children);
 *	parent->num_children = 0;
 */
#define tlist_init(h) list_head_init(&(h)->raw)

/**
 * tlist_raw - unwrap the typed list and check the type
 * @h: the tlist
 * @expr: the expression to check the type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying list.
 */
#define tlist_raw(h, expr) (&tcon_check((h), canary, (expr))->raw)

/**
 * tlist_add - add an entry at the start of a linked list.
 * @h: the tlist to add the node to
 * @n: the entry to add to the list.
 * @member: the member of n to add to the list.
 *
 * The entry's list_node does not need to be initialized; it will be
 * overwritten.
 * Example:
 *	struct child *child = malloc(sizeof(*child));
 *
 *	child->name = "marvin";
 *	tlist_add(&parent->children, child, list);
 *	parent->num_children++;
 */
#define tlist_add(h, n, member) list_add(tlist_raw((h), (n)), &(n)->member)

/**
 * tlist_add_tail - add an entry at the end of a linked list.
 * @h: the tlist to add the node to
 * @n: the entry to add to the list.
 * @member: the member of n to add to the list.
 *
 * The list_node does not need to be initialized; it will be overwritten.
 * Example:
 *	tlist_add_tail(&parent->children, child, list);
 *	parent->num_children++;
 */
#define tlist_add_tail(h, n, member) \
	list_add_tail(tlist_raw((h), (n)), &(n)->member)

/**
 * tlist_del_from - delete an entry from a linked list.
 * @h: the tlist @n is in
 * @n: the entry to delete
 * @member: the member of n to remove from the list.
 *
 * This explicitly indicates which list a node is expected to be in,
 * which is better documentation and can catch more bugs.
 *
 * Note that this leaves @n->@member in an undefined state; it
 * can be added to another list, but not deleted again.
 *
 * See also: tlist_del()
 *
 * Example:
 *	tlist_del_from(&parent->children, child, list);
 *	parent->num_children--;
 */
#define tlist_del_from(h, n, member) \
	list_del_from(tlist_raw((h), (n)), &(n)->member)

/**
 * tlist_del - delete an entry from an unknown linked list.
 * @n: the entry to delete from the list.
 * @member: the member of @n which is in the list.
 *
 * Example:
 *	tlist_del(child, list);
 *	parent->num_children--;
 */
#define tlist_del(n, member) \
	list_del(&(n)->member)

/**
 * tlist_empty - is a list empty?
 * @h: the tlist
 *
 * If the list is empty, returns true.
 *
 * Example:
 *	assert(tlist_empty(&parent->children) == (parent->num_children == 0));
 */
#define tlist_empty(h) list_empty(&(h)->raw)

/**
 * tlist_top - get the first entry in a list
 * @h: the tlist
 * @member: the list_node member of the type
 *
 * If the list is empty, returns NULL.
 *
 * Example:
 *	struct child *first;
 *	first = tlist_top(&parent->children, list);
 */
#define tlist_top(h, member)						\
	((tcon_type((h), canary))					\
	 list_top_(&(h)->raw,						\
		   (char *)(&(h)->_tcon[0].canary->member) -		\
		   (char *)((h)->_tcon[0].canary)))

/**
 * tlist_tail - get the last entry in a list
 * @h: the tlist
 * @member: the list_node member of the type
 *
 * If the list is empty, returns NULL.
 *
 * Example:
 *	struct child *last;
 *	last = tlist_tail(&parent->children, list);
 */
#define tlist_tail(h, member)						\
	((tcon_type((h), canary))					\
	 list_tail_(&(h)->raw,						\
		    (char *)(&(h)->_tcon[0].canary->member) -		\
		    (char *)((h)->_tcon[0].canary)))

/**
 * tlist_for_each - iterate through a list.
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 * @member: the list_node member of @i
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.
 *
 * Example:
 *	tlist_for_each(&parent->children, child, list)
 *		printf("Name: %s\n", child->name);
 */
#define tlist_for_each(h, i, member)					\
	list_for_each(tlist_raw((h), (i)), (i), member)

/**
 * tlist_for_each - iterate through a list backwards.
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 * @member: the list_node member of @i
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.
 *
 * Example:
 *	tlist_for_each_rev(&parent->children, child, list)
 *		printf("Name: %s\n", child->name);
 */
#define tlist_for_each_rev(h, i, member)					\
	list_for_each_rev(tlist_raw((h), (i)), (i), member)

/**
 * tlist_for_each_safe - iterate through a list, maybe during deletion
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 * @nxt: another iterator to store the next entry.
 * @member: the list_node member of the structure
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.  The extra variable
 * @nxt is used to hold the next element, so you can delete @i from the list.
 *
 * Example:
 *	struct child *next;
 *	tlist_for_each_safe(&parent->children, child, next, list) {
 *		tlist_del(child, list);
 *		parent->num_children--;
 *	}
 */
#define tlist_for_each_safe(h, i, nxt, member)				\
	list_for_each_safe(tlist_raw((h), (i)), (i), (nxt), member)

#endif /* CCAN_TLIST_H */
