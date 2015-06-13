/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_LQUEUE_H
#define CCAN_LQUEUE_H

#include <stdbool.h>
#include <stdio.h>
#include <assert.h>

#include <ccan/container_of/container_of.h>

/**
 * struct lqueue_link - a queue link
 * @next: next entry, or front of queue, if this is the back
 *
 * This is used as a link within a queue entry.
 *
 * Example:
 *	struct waiter {
 *		char *name;
 *		struct lqueue_link ql;
 *	};
 */
struct lqueue_link {
	struct lqueue_link *next;
};

/**
 * struct lqueue - the head of a queue
 * @b: the back of the queue (NULL if empty)
 */
struct lqueue {
	struct lqueue_link *back;
};

/**
 * LQUEUE - define and initialize an empty queue
 * @name: the name of the lqueue.
 *
 * The LQUEUE macro defines an lqueue and initializes it to an empty
 * queue.  It can be prepended by "static" to define a static lqueue.
 *
 * See also:
 *	lqueue_init()
 *
 * Example:
 *	LQUEUE(my_queue);
 *
 *	assert(lqueue_empty(&my_queue));
 */
#define LQUEUE(name) \
	struct lqueue name = { NULL, }

/**
 * lqueue_init_from_back - initialize a queue with a specific back element
 * @s: the lqueue to initialize
 * @e: pointer to the back element of the new queue
 * @member: member of the element containing the lqueue_link
 *
 * USE WITH CAUTION: This is for handling unusual cases where you have
 * a pointer to an element in a previously constructed queue but can't
 * conveniently pass around a normal struct lqueue.  Usually you
 * should use lqueue_init().
 *
 * Example:
 *	LQUEUE(queue1);
 *	struct lqueue queue2;
 *	struct element {
 *		int value;
 *		struct lqueue_link link;
 *	} el;
 *
 *	lqueue_enqueue(&queue1, &el, link);
 *
 *	lqueue_init_from_back(&queue2,
 *	                      lqueue_back(&queue1, struct element, link), link);
 */
#define lqueue_init_from_back(s, e, member) \
	(lqueue_init_((s), &(e)->member))

/**
 * lqueue_init - initialize a queue
 * @h: the lqueue to set to an empty queue
 *
 * Example:
 *	struct lqueue *qp = malloc(sizeof(*qp));
 *	lqueue_init(qp);
 */
#define lqueue_init(s) \
	(lqueue_init_((s), NULL))
static inline void lqueue_init_(struct lqueue *q, struct lqueue_link *back)
{
	q->back = back;
}

/**
 * lqueue_empty - is a queue empty?
 * @q: the queue
 *
 * If the queue is empty, returns true.
 *
 * Example:
 *	assert(lqueue_empty(qp));
 */
static inline bool lqueue_empty(const struct lqueue *q)
{
	return (q->back == NULL);
}

/**
 * lqueue_entry - convert an lqueue_link back into the structure containing it.
 * @e: the lqueue_link
 * @type: the type of the entry
 * @member: the lqueue_link member of the type
 *
 * Example:
 *	struct waiter {
 *		char *name;
 *		struct lqueue_link ql;
 *	} w;
 *	assert(lqueue_entry(&w.ql, struct waiter, ql) == &w);
 */
#define lqueue_entry(n, type, member) container_of_or_null(n, type, member)

/**
 * lqueue_front - get front entry in a queue
 * @q: the queue
 * @type: the type of queue entries
 * @member: the lqueue_link entry
 *
 * If the queue is empty, returns NULL.
 *
 * Example:
 *	struct waiter *f;
 *
 *	f = lqueue_front(qp, struct waiter, ql);
 *	assert(lqueue_dequeue(qp, struct waiter, ql) == f);
 */
#define lqueue_front(q, type, member) \
	lqueue_entry(lqueue_front_((q)), type, member)
static inline struct lqueue_link *lqueue_front_(const struct lqueue *q)
{
	if (!q->back)
		return NULL;
	else
		return q->back->next;
}

/**
 * lqueue_back - get back entry in a queue
 * @q: the queue
 * @type: the type of queue entries
 * @member: the lqueue_link entry
 *
 * If the queue is empty, returns NULL.
 *
 * Example:
 *	struct waiter b;
 *
 *	lqueue_enqueue(qp, &b, ql);
 *	assert(lqueue_back(qp, struct waiter, ql) == &b);
 */
#define lqueue_back(q, type, member) \
	lqueue_entry(lqueue_back_((q)), type, member)
static inline struct lqueue_link *lqueue_back_(const struct lqueue *q)
{
	return q->back;
}

/**
 * lqueue_enqueue - add an entry to the back of a queue
 * @q: the queue to add the node to
 * @e: the item to enqueue
 * @member: the lqueue_link field of *e
 *
 * The lqueue_link does not need to be initialized; it will be overwritten.
 */
#define lqueue_enqueue(q, e, member) \
	lqueue_enqueue_((q), &((e)->member))
static inline void lqueue_enqueue_(struct lqueue *q, struct lqueue_link *e)
{
	if (lqueue_empty(q)) {
		/* New entry will be both front and back of queue */
		e->next = e;
		q->back = e;
	} else {
		e->next = lqueue_front_(q);
		q->back->next = e;
		q->back = e;
	}
}

/**
 * lqueue_dequeue - remove and return the entry from the front of the queue
 * @q: the queue
 * @type: the type of queue entries
 * @member: the lqueue_link field of @type
 *
 * Note that this leaves the returned entry's link in an undefined
 * state; it can be added to another queue, but not deleted again.
 */
#define lqueue_dequeue(q, type, member) \
	lqueue_entry(lqueue_dequeue_((q)), type, member)
static inline struct lqueue_link *lqueue_dequeue_(struct lqueue *q)
{
	struct lqueue_link *front;

	if (lqueue_empty(q))
		return NULL;

	front = lqueue_front_(q);
	if (front == lqueue_back_(q)) {
		assert(front->next == front);
		q->back = NULL;
	} else {
		q->back->next = front->next;
	}
	return front;
}

#endif /* CCAN_LQUEUE_H */
