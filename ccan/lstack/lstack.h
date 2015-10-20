/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_LSTACK_H
#define CCAN_LSTACK_H

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <ccan/tcon/tcon.h>

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
 * struct lstack_ - a stack (internal type)
 * @b: the top of the stack (NULL if empty)
 */
struct lstack_ {
	struct lstack_link *top;
};

/**
 * LSTACK - declare a stack
 * @type: the type of elements in the stack
 * @link: the field containing the lstack_link in @type
 *
 * The LSTACK macro declares an lstack.  It can be prepended by
 * "static" to define a static lstack.  The stack begins in undefined
 * state, you must either initialize with LSTACK_INIT, or call
 * lstack_init() before using it.
 *
 * See also:
 *	lstack_init()
 *
 * Example:
 *	struct element {
 *		int value;
 *		struct lstack_link link;
 *	};
 *	LSTACK(struct element, link) my_stack;
 */
#define LSTACK(etype, link)						\
	TCON_WRAP(struct lstack_,					\
		  TCON_CONTAINER(canary, etype, link))

/**
 * LSTACK_INIT - initializer for an empty stack
 *
 * The LSTACK_INIT macro returns a suitable initializer for a stack
 * defined with LSTACK.
 *
 * Example:
 *	struct element {
 *		int value;
 *		struct lstack_link link;
 *	};
 *	LSTACK(struct element, link) my_stack = LSTACK_INIT;
 *
 *	assert(lstack_empty(&my_stack));
 */
#define LSTACK_INIT				\
	TCON_WRAP_INIT({ NULL, })

/**
 * lstack_entry - convert an lstack_link back into the structure containing it.
 * @s: the stack
 * @l: the lstack_link
 *
 * Example:
 *	struct element {
 *		int value;
 *		struct lstack_link link;
 *	} e;
 *	LSTACK(struct element, link) my_stack;
 *	assert(lstack_entry(&my_stack, &e.link) == &e);
 */
#define lstack_entry(s_, l_) tcon_container_of((s_), canary, (l_))


/**
 * lstack_init_from_top - initialize a stack with a given top element
 * @s: the lstack to initialize
 * @e: pointer to the top element of the new stack
 *
 * USE WITH CAUTION: This is for handling unusual cases where you have
 * a pointer to an element in a previously constructed stack but can't
 * conveniently pass around a normal struct lstack.  Usually you
 * should use lstack_init().
 *
 * Example:
 *	struct element {
 *		int value;
 *		struct lstack_link link;
 *	} e;
 *	LSTACK(struct element, link) stack1 = LSTACK_INIT;
 *	LSTACK(struct element, link) stack2;
 *
 *	lstack_push(&stack1, &e);
 *
 *	lstack_init_from_top(&stack2, lstack_top(&stack1));
 */
#define lstack_init_from_top(s_, e_)	\
	(lstack_init_(tcon_unwrap(s_), tcon_member_of((s_), canary, (e_))))

/**
 * lstack_init - initialize a stack
 * @h: the lstack to set to an empty stack
 *
 * Example:
 *	struct element {
 *		int value;
 *		struct lstack_link link;
 *	};
 *	LSTACK(struct element, link) *sp = malloc(sizeof(*sp));
 *	lstack_init(sp);
 */
#define lstack_init(s_) \
	(lstack_init_(tcon_unwrap(s_), NULL))
static inline void lstack_init_(struct lstack_ *s, struct lstack_link *top)
{
	s->top = top;
}

/**
 * lstack_empty - is a stack empty?
 * @s: the stack
 *
 * If the stack is empty, returns true.
 */
#define lstack_empty(s_) \
	lstack_empty_(tcon_unwrap(s_))
static inline bool lstack_empty_(const struct lstack_ *s)
{
	return (s->top == NULL);
}

/**
 * lstack_top - get top entry in a stack
 * @s: the stack
 *
 * If the stack is empty, returns NULL.
 *
 * Example:
 *	struct element *t;
 *
 *	t = lstack_top(sp);
 *	assert(lstack_pop(sp) == t);
 */
#define lstack_top(s_) \
	lstack_entry((s_), lstack_top_(tcon_unwrap(s_)))
static inline struct lstack_link *lstack_top_(const struct lstack_ *s)
{
	return s->top;
}

/**
 * lstack_push - add an entry to the top of the stack
 * @s: the stack to add the node to
 * @e: the item to push
 *
 * The lstack_link does not need to be initialized; it will be overwritten.
 */
#define lstack_push(s_, e_) \
	lstack_push_(tcon_unwrap(s_), tcon_member_of((s_), canary, (e_)))
static inline void lstack_push_(struct lstack_ *s, struct lstack_link *e)
{
	e->down = lstack_top_(s);
	s->top = e;
}

/**
 * lstack_pop - remove and return the entry from the top of the stack
 * @s: the stack
 *
 * Note that this leaves the returned entry's link in an undefined
 * state; it can be added to another stack, but not deleted again.
 */
#define lstack_pop(s_)					\
	lstack_entry((s_), lstack_pop_(tcon_unwrap((s_))))
static inline struct lstack_link *lstack_pop_(struct lstack_ *s)
{
	struct lstack_link *top;

	if (lstack_empty_(s))
		return NULL;

	top = lstack_top_(s);
	s->top = top->down;
	return top;
}

#endif /* CCAN_LSTACK_H */
