/* Licensed under LGPL - see LICENSE file for details */
#ifndef CCAN_TLIST2_H
#define CCAN_TLIST2_H
#include <ccan/list/list.h>
#include <ccan/tcon/tcon.h>

/**
 * TLIST2 - declare a typed list type (struct tlist)
 * @etype: the type the list will contain
 * @link: the name of the member of @etype that is the link
 *
 * This declares an anonymous structure to use for lists containing this type.
 * The actual list can be accessed using tlist2_raw().
 *
 * Example:
 *	#include <ccan/list/list.h>
 *	#include <ccan/tlist2/tlist2.h>
 *	struct child {
 *		const char *name;
 *		struct list_node list;
 *	};
 *	struct parent {
 *		const char *name;
 *		TLIST2(struct child, list) children;
 *		unsigned int num_children;
 *	};
 *
 */
#define TLIST2(etype, link)				\
	TCON_WRAP(struct list_head,			\
		TCON_CONTAINER(canary, etype, link))

/**
 * TLIST2_INIT - initalizer for an empty tlist
 * @name: the name of the list.
 *
 * Explicit initializer for an empty list.
 *
 * See also:
 *	tlist2_init()
 *
 * Example:
 *	TLIST2(struct child, list) my_list = TLIST2_INIT(my_list);
 */
#define TLIST2_INIT(name) TCON_WRAP_INIT( LIST_HEAD_INIT(*tcon_unwrap(&(name))) )

/**
 * tlist2_check - check head of a list for consistency
 * @h: the tlist2 head
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
 *		tlist2_check(&p->children, "bad child list");
 *		tlist2_for_each(&p->children, c)
 *			printf(" -> %s\n", c->name);
 *	}
 */
#define tlist2_check(h, abortstr) \
	list_check(tcon_unwrap(h), (abortstr))

/**
 * tlist2_init - initialize a tlist
 * @h: the tlist to set to the empty list
 *
 * Example:
 *	...
 *	struct parent *parent = malloc(sizeof(*parent));
 *
 *	tlist2_init(&parent->children);
 *	parent->num_children = 0;
 */
#define tlist2_init(h) list_head_init(tcon_unwrap(h))

/**
 * tlist2_raw - unwrap the typed list and check the type
 * @h: the tlist
 * @expr: the expression to check the type against (not evaluated)
 *
 * This macro usually causes the compiler to emit a warning if the
 * variable is of an unexpected type.  It is used internally where we
 * need to access the raw underlying list.
 */
#define tlist2_raw(h, expr) tcon_unwrap(tcon_container_check_ptr(h, canary, expr))

/**
 * tlist2_unwrap - unwrap the typed list without any checks
 * @h: the tlist
 */
#define tlist2_unwrap(h) tcon_unwrap(h)

/**
 * tlist2_add - add an entry at the start of a linked list.
 * @h: the tlist to add the node to
 * @n: the entry to add to the list.
 *
 * The entry's list_node does not need to be initialized; it will be
 * overwritten.
 * Example:
 *	struct child *child = malloc(sizeof(*child));
 *
 *	child->name = "marvin";
 *	tlist2_add(&parent->children, child);
 *	parent->num_children++;
 */
#define tlist2_add(h, n) list_add(tlist2_raw((h), (n)), tcon_member_of(h, canary, n))

/**
 * tlist2_add_tail - add an entry at the end of a linked list.
 * @h: the tlist to add the node to
 * @n: the entry to add to the list.
 *
 * The list_node does not need to be initialized; it will be overwritten.
 * Example:
 *	tlist2_add_tail(&parent->children, child);
 *	parent->num_children++;
 */
#define tlist2_add_tail(h, n) \
	list_add_tail(tlist2_raw((h), (n)), tcon_member_of((h), canary, (n)))

/**
 * tlist2_del_from - delete an entry from a linked list.
 * @h: the tlist @n is in
 * @n: the entry to delete
 *
 * This explicitly indicates which list a node is expected to be in,
 * which is better documentation and can catch more bugs.
 *
 * Note that this leaves @n->@member in an undefined state; it
 * can be added to another list, but not deleted again.
 *
 * Example:
 *	tlist2_del_from(&parent->children, child);
 *	parent->num_children--;
 */
#define tlist2_del_from(h, n) \
	list_del_from(tlist2_raw((h), (n)), tcon_member_of((h), canary, (n)))

/**
 * tlist2_empty - is a list empty?
 * @h: the tlist
 *
 * If the list is empty, returns true.
 *
 * Example:
 *	assert(tlist2_empty(&parent->children) == (parent->num_children == 0));
 */
#define tlist2_empty(h) list_empty(tcon_unwrap(h))

/**
 * tlist2_top - get the first entry in a list
 * @h: the tlist
 *
 * If the list is empty, returns NULL.
 *
 * Example:
 *	struct child *first;
 *	first = tlist2_top(&parent->children);
 *	if (!first)
 *		printf("Empty list!\n");
 */
#define tlist2_top(h) tcon_container_of((h), canary, list_top_(tcon_unwrap(h), 0))

/**
 * tlist2_tail - get the last entry in a list
 * @h: the tlist
 *
 * If the list is empty, returns NULL.
 *
 * Example:
 *	struct child *last;
 *	last = tlist2_tail(&parent->children);
 *	if (!last)
 *		printf("Empty list!\n");
 */
#define tlist2_tail(h) tcon_container_of((h), canary, list_tail_(tcon_unwrap(h), 0))

/**
 * tlist2_for_each - iterate through a list.
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.
 *
 * Example:
 *	tlist2_for_each(&parent->children, child)
 *		printf("Name: %s\n", child->name);
 */
#define tlist2_for_each(h, i)					\
	list_for_each_off(tlist2_raw((h), (i)), (i), tcon_offset((h), canary))

/**
 * tlist2_for_each_rev - iterate through a list backwards.
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.
 *
 * Example:
 *	tlist2_for_each_rev(&parent->children, child)
 *		printf("Name: %s\n", child->name);
 */
#define tlist2_for_each_rev(h, i)					\
	list_for_each_rev_off(tlist2_raw((h), (i)), (i), tcon_offset((h), canary))

/**
 * tlist2_for_each_safe - iterate through a list, maybe during deletion
 * @h: the tlist
 * @i: an iterator of suitable type for this list.
 * @nxt: another iterator to store the next entry.
 *
 * This is a convenient wrapper to iterate @i over the entire list.  It's
 * a for loop, so you can break and continue as normal.  The extra variable
 * @nxt is used to hold the next element, so you can delete @i from the list.
 *
 * Example:
 *	struct child *next;
 *	tlist2_for_each_safe(&parent->children, child, next) {
 *		tlist2_del_from(&parent->children, child);
 *		parent->num_children--;
 *	}
 */
#define tlist2_for_each_safe(h, i, nxt)				\
	list_for_each_safe_off(tlist2_raw((h), (i)), (i), (nxt), tcon_offset((h), canary))

#endif /* CCAN_TLIST2_H */
