/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_LSTACK_H
#define CCAN_LSTACK_H

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <ccan/container_of/container_of.h>

/**
 * struct lstack_link - a stack link
 * @down: immedately lower entry in the stack, or NULL if this is the bottom.
 *
 * This is used as an entry in a stack.
 *
 * Example:
 *	struct stacker {
 *		char *name;
 *		struct lstack_link sl;
 *	};
 */
struct lstack_link {
	struct lstack_link *down;
};

/**
 * struct lstack - a stack
 * @b: the top of the stack (NULL if empty)
 */
struct lstack {
	struct lstack_link *top;
};

/**
 * LSTACK - define and initialize an empty stack
 * @name: the name of the lstack.
 *
 * The LSTACK macro defines an lstack and initializes it to an empty
 * stack.  It can be prepended by "static" to define a static lstack.
 *
 * See also:
 *	lstack_init()
 *
 * Example:
 *	LSTACK(my_stack);
 *
 *	assert(lstack_empty(&my_stack));
 */
#define LSTACK(name) \
	struct lstack name = { NULL, }

/**
 * lstack_init - initialize a stack
 * @h: the lstack to set to an empty stack
 *
 * Example:
 *	struct lstack *sp = malloc(sizeof(*sp));
 *	lstack_init(sp);
 */
static inline void lstack_init(struct lstack *s)
{
	s->top = NULL;
}

/**
 * lstack_empty - is a stack empty?
 * @s: the stack
 *
 * If the stack is empty, returns true.
 *
 * Example:
 *	assert(lstack_empty(sp));
 */
static inline bool lstack_empty(const struct lstack *s)
{
	return (s->top == NULL);
}

/**
 * lstack_entry - convert an lstack_link back into the structure containing it.
 * @e: the lstack_link
 * @type: the type of the entry
 * @member: the lstack_link member of the type
 *
 * Example:
 *	struct stacker {
 *		char *name;
 *		struct lstack_link sl;
 *	} st;
 *	assert(lstack_entry(&st.sl, struct stacker, sl) == &st);
 */
#define lstack_entry(n, type, member) container_of_or_null(n, type, member)

/**
 * lstack_top - get top entry in a stack
 * @s: the stack
 * @type: the type of stack entries
 * @member: the lstack_link entry
 *
 * If the stack is empty, returns NULL.
 *
 * Example:
 *	struct stacker *t;
 *
 *	t = lstack_top(sp, struct stacker, sl);
 *	assert(lstack_pop(sp, struct stacker, sl) == t);
 */
#define lstack_top(s, type, member) \
	lstack_entry(lstack_top_((s)), type, member)
static inline struct lstack_link *lstack_top_(const struct lstack *s)
{
	return s->top;
}

/**
 * lstack_push - add an entry to the top of the stack
 * @s: the stack to add the node to
 * @e: the item to push
 * @member: the lstack_link field of *e
 *
 * The lstack_link does not need to be initialized; it will be overwritten.
 */
#define lstack_push(s, e, member) \
	lstack_push_((s), &((e)->member))
static inline void lstack_push_(struct lstack *s, struct lstack_link *e)
{
	e->down = lstack_top_(s);
	s->top = e;
}

/**
 * lstack_pop - remove and return the entry from the top of the stack
 * @s: the stack
 * @type: the type of stack entries
 * @member: the lstack_link field of @type
 *
 * Note that this leaves the returned entry's link in an undefined
 * state; it can be added to another stack, but not deleted again.
 */
#define lstack_pop(s, type, member) \
	lstack_entry(lstack_pop_((s)), type, member)
static inline struct lstack_link *lstack_pop_(struct lstack *s)
{
	struct lstack_link *top;

	if (lstack_empty(s))
		return NULL;

	top = lstack_top_(s);
	s->top = top->down;
	return top;
}

#endif /* CCAN_LSTACK_H */
