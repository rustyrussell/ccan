/* BSD-MIT: See LICENSE file for details. */
#ifndef CCAN_ANTITHREAD_QUEUE_H
#define CCAN_ANTITHREAD_QUEUE_H
#include <stdlib.h>

#define QUEUE_ELEMS 32
struct queue {
	unsigned int head;
	unsigned int prod_waiting;
	struct notify *produced;
	unsigned int prod_lock;
	void *elems[QUEUE_ELEMS];
	unsigned int tail;
	unsigned int cons_waiting;
	struct notify *consumed;
};

/**
 * queue_size - get queue size in bytes for given number of elements.
 * @num: number of elements.
 */
size_t queue_size(size_t num);

/**
 * queue_init - initialize queue in memory
 * @q: the memory.
 * @prod: notifier when something is produced.
 * @cons: notifier when something is consumed.
 */
void queue_init(struct queue *q, struct notify *prod, struct notify *cons);

/**
 * queue_insert - add an element to the queue
 * @q: the queue
 * @ptr: the pointer to add
 */
void queue_insert(struct queue *q, void *elem);

/**
 * queue_remove - remove an element to the queue
 * @q: the queue
 */
void *queue_remove(struct queue *q);

#endif /* CCAN_ANTITHREAD_QUEUE_H */
