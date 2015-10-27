/* GNU LGPL version 2 (or later) - see LICENSE file for details */
#ifndef CCAN_LPQ_H
#define CCAN_LPQ_H

#include <stdbool.h>

#include <ccan/cast/cast.h>
#include <ccan/tcon/tcon.h>
#include <ccan/order/order.h>

/**
 * struct lpq_link - a priority link
 * @next: next entry in the list of items in the priority queue
 *
 * This is used as a link within a priority queue entry.
 *
 * Example:
 *	struct waiter {
 *		char *name;
 *		int priority;
 *		struct lpq_link pql;
 *	};
 */
struct lpq_link {
	struct lpq_link *next;
};

/**
 * struct lpq_ - a linear priority queue (internal type)
 * @order: ordering callback to compare entry priorities
 * @list: head of the list of items in the priority queue
 */
struct lpq_ {
	struct _total_order order;
	struct lpq_link *list;
};

/**
 * LPQ - define and initialize an empty priority queue
 * @type: type of items in the queue / items compared by the callback
 * @link: name of the lpq_link field in @type
 *
 * The LPQ macro defines an lpq and initializes it to an empty
 * priority queue.  It can be prepended by "static" to define a static
 * lpq.
 *
 * See also:
 *	lpq_init()
 */
#define LPQ(etype_, link_)				\
	TCON_WRAP(struct lpq_,				\
		  TCON_CONTAINER(canary, etype_, link_))

/**
 * lpq_init - initialize a priority queue
 * @pq: the lpq to set to an empty queue
 * @order: priority ordering callback for items in the queue
 *
 * Example:
 *	total_order_by_field(my_order, int, struct waiter, priority);
 *	LPQ(struct waiter, pql) *pqp = malloc(sizeof(*pqp));
 *	lpq_init(pqp, my_order.cb, my_order.ctx);
 */
#define lpq_init(pq_, order_cb_, order_ctx_)				\
	lpq_init_(tcon_unwrap(pq_),					\
		  total_order_cast((order_cb_),				\
				   tcon_container_type((pq_), canary),	\
				   (order_ctx_)),			\
		  (order_ctx_))
static inline void lpq_init_(struct lpq_ *pq,
			     _total_order_cb order_cb, void *order_ctx)
{
	pq->order.cb = order_cb;
	pq->order.ctx = order_ctx;
	pq->list = NULL;
}

/**
 * lpq_empty - is a priority queue empty?
 * @pq: the priority queue
 *
 * If the priority queue is empty, returns true.
 */
#define lpq_empty(pq_) \
	lpq_empty_(tcon_unwrap(pq_))
static inline bool lpq_empty_(const struct lpq_ *pq)
{
	return (pq->list == NULL);
}

/**
 * lpq_entry - convert an lpq_link back into the structure containing it.
 * @pq: the priority queue
 * @l: the lpq_link
 */
#define lpq_entry(pq_, l_) tcon_container_of((pq_), canary, (l_))

/**
 * lpq_frontp_ - get pointer to pointer to front element (internal function)
 */
struct lpq_link **lpq_frontp_(struct lpq_ *pq, size_t offset);

/**
 * lpq_front - get front (highest priority) entry in a priority queue
 * @pq: the priority queue
 *
 * If the priority queue is empty, returns NULL.
 *
 * Example:
 *	struct waiter *f;
 *
 *	f = lpq_front(pqp);
 *	assert(lpq_dequeue(pqp) == f);
 */
#define lpq_front(pq_) \
	lpq_entry((pq_), lpq_front_(tcon_unwrap(pq_), \
				    tcon_offset((pq_), canary)))
static inline struct lpq_link *lpq_front_(const struct lpq_ *pq, size_t offset)
{
	struct lpq_link **frontp = lpq_frontp_(cast_const(struct lpq_ *, pq),
					       offset);

	return frontp ? *frontp : NULL;
}

/**
 * lpq_enqueue - add an entry to a priority queue
 * @pq: the priority queue to add the node to
 * @e: the item to enqueue
 *
 * The lpq_link does not need to be initialized; it will be overwritten.
 */
#define lpq_enqueue(pq_, e_)			\
	lpq_enqueue_(tcon_unwrap(pq_), tcon_member_of((pq_), canary, (e_)))
static inline void lpq_enqueue_(struct lpq_ *pq, struct lpq_link *e)
{
	e->next = pq->list;
	pq->list = e;
}

/**
 * lpq_dequeue - remove and return the highest priority item from the
 *               priority queue
 * @pq: the priority queue
 *
 * Note that this leaves the returned entry's link in an undefined
 * state; it can be added to another queue, but not deleted again.
 */
#define lpq_dequeue(pq_) \
	lpq_entry((pq_), lpq_dequeue_(tcon_unwrap(pq_),	\
				      tcon_offset((pq_), canary)))
static inline struct lpq_link *lpq_dequeue_(struct lpq_ *pq, size_t offset)
{
	struct lpq_link **frontp = lpq_frontp_(pq, offset);
	struct lpq_link *front;

	if (!frontp)
		return NULL;

	front = *frontp;
	*frontp = front->next;
	return front;
}

/**
 * lpq_reorder - adjust the queue after an element changes priority
 * @pq: the priority queue
 * @e: the entry which has changed priority
 *
 * If any element already inserted into @pq is altered to change its
 * priority, lpq_reorder() must be called before any other function is
 * called on @pq.
 *
 * NOTE: For the dumb priority queue implementation in lpq, this is
 * actually a no-op.  But this call exists so that users will be more
 * easily able to change to a better priority queue implementation
 * later.
 */
#define lpq_reorder(pq_, e_) \
	(lpq_reorder_(tcon_unwrap(pq_), tcon_member_of((pq_), canary, (e_))))
static inline void lpq_reorder_(struct lpq_ *pq, struct lpq_link *e)
{
}

#endif /* CCAN_LPQ_H */
