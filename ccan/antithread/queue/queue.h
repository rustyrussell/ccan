/* BSD-MIT: See LICENSE file for details. */
#ifndef CCAN_ANTITHREAD_QUEUE_H
#define CCAN_ANTITHREAD_QUEUE_H
#include <stdlib.h>

#define QUEUE_ELEMS 32
struct queue {
	unsigned int head;
	unsigned int prod_waiting;
	unsigned int prod_lock;
	void *elems[QUEUE_ELEMS];
	unsigned int tail;
	unsigned int cons_waiting;
};

/**
 * queue_size - get queue size in bytes for given number of elements.
 * @num: number of elements.
 */
size_t queue_size(size_t num);

/**
 * queue_init - initialize queue in memory
 * @q: the memory.
 */
void queue_init(struct queue *q);

/**
 * queue_insert - add an element to the queue
 * @q: the queue
 * @ptr: the pointer to add
 */
void queue_insert(struct queue *q, void *elem);

/**
 * queue_insert_excl - add an element to the queue (exclusive)
 * @q: the queue
 * @ptr: the pointer to add
 *
 * Use this if you know that no other process will be inserting in the queue.
 */
void queue_insert_excl(struct queue *q, void *elem);

/**
 * queue_remove_excl - remove an element to the queue (exclusive)
 * @q: the queue
 *
 * Use this if you know that no other process will be removing from the queue.
 */
void *queue_remove_excl(struct queue *q);

/**
 * queue_remove - remove an element to the queue
 * @q: the queue
 */
void *queue_remove(struct queue *q);

#endif /* CCAN_ANTITHREAD_QUEUE_H */
