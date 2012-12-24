/* BSD-MIT: See LICENSE file for details. */
#include <ccan/antithread/queue/queue.h>
#include "config.h"
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#if HAVE_GCC_ATOMICS
/* Even these will go away with stdatomic.h */
static unsigned int read_once(unsigned int *ptr, int memmodel)
{
	return __atomic_load_n(ptr, memmodel);
}

static void *read_ptr(void **ptr, int memmodel)
{
	return __atomic_load_n(ptr, memmodel);
}

static void store_once(unsigned int *ptr, unsigned int val, int memmodel)
{
	__atomic_store_n(ptr, val, memmodel);
}

static void store_ptr(void **ptr, void *val, int memmodel)
{
	__atomic_store_n(ptr, val, memmodel);
}

static void atomic_inc(unsigned int *val, int memmodel)
{
	__atomic_add_fetch(val, 1, memmodel);
}

static void atomic_dec(unsigned int *val, int memmodel)
{
	__atomic_sub_fetch(val, 1, memmodel);
}

static bool compare_and_swap(unsigned int *ptr,
			     unsigned int old, unsigned int new, int memmodel)
{
	return __atomic_compare_exchange_n(ptr, &old, new, false,
					   memmodel, memmodel);
}
#else
#undef __ATOMIC_SEQ_CST
#undef __ATOMIC_RELAXED
#undef __ATOMIC_ACQUIRE
#undef __ATOMIC_RELEASE

/* Overkill, but all-or-nothing keeps it simple. */
#define __ATOMIC_SEQ_CST 1
#define __ATOMIC_RELAXED 1
#define __ATOMIC_ACQUIRE 1
#define __ATOMIC_RELEASE 0

#ifdef __i386__
static inline void mb(void)
{
	asm volatile ("mfence" : : : "memory");
}
#else
#error implement mb
#endif

static unsigned int read_once(unsigned int *ptr, bool barrier)
{
	if (barrier)
		mb();
	return *(volatile unsigned int *)ptr;
}

static void *read_ptr(void **ptr, bool barrier)
{
	if (barrier)
		mb();
	return *(void * volatile *)ptr;
}

static void store_once(unsigned int *ptr, unsigned int val, bool barrier)
{
	*(volatile unsigned int *)ptr = val;
	if (barrier)
		mb();
}

static void store_ptr(void **ptr, void *val, bool barrier)
{
	*(void * volatile *)ptr = val;
	if (barrier)
		mb();
}

static void atomic_inc(unsigned int *val, bool barrier)
{
#ifdef __i386__
	asm volatile ("lock addl $1, (%0)" : : "r"(val) : "memory");
#else
#error implement atomic_inc
#endif
}

static void atomic_dec(unsigned int *val, bool barrier)
{
#ifdef __i386__
	asm volatile ("lock subl $1, (%0)" : : "r"(val) : "memory");
#else
#error implement atomic_dec
#endif
}

static bool compare_and_swap(unsigned int *ptr,
			     unsigned int old, unsigned int new,
			     int memmodel)
{
#ifdef __i386__
	unsigned int prev;

	asm volatile ("lock cmpxchgl %1, (%2)"
		      : "=a"(prev) : "r"(new), "r"(ptr), "a"(old) : "memory");
	return prev == old;
#else
#error implement compare_and_swap
#endif
}
#endif /* ! GCC 4.7 or above */

static void wait_for_change(unsigned int *ptr, unsigned int val)
{
	while (read_once(ptr, __ATOMIC_RELAXED) == val);
}


static void wake_consumer(struct queue *q)
{
}

static void wake_producer(struct queue *q)
{
}

void queue_init(struct queue *q)
{
	memset(q->elems, 0xFF, sizeof(q->elems));
	q->prod_waiting = q->prod_lock = 0;
	q->tail = q->cons_waiting = 0;
	/* We need at least one barrier here. */
	store_once(&q->head, 0, __ATOMIC_SEQ_CST);
}

/* We are the only producer accessing the queue. */
void queue_insert_excl(struct queue *q, void *elem)
{
	unsigned int t, h;

	h = read_once(&q->head, __ATOMIC_RELAXED);
	t = read_once(&q->tail, __ATOMIC_RELAXED);
	assert(!(h & 1));

	if (h == t + QUEUE_ELEMS * 2) {
		atomic_inc(&q->prod_waiting, __ATOMIC_SEQ_CST);
		wait_for_change(&q->tail, t);
		atomic_dec(&q->prod_waiting, __ATOMIC_RELAXED);
	}

	/* Make sure contents of elem are written first, and head is
	 * written afterwards. */
	store_ptr(&q->elems[(h/2) % QUEUE_ELEMS], elem, __ATOMIC_SEQ_CST);
	assert(read_once(&q->head, __ATOMIC_RELAXED) == h);
	store_once(&q->head, h+2, __ATOMIC_SEQ_CST);
	if (q->cons_waiting)
		wake_consumer(q);
	return;
}

/* We are the only consumer accessing the queue. */
void *queue_remove_excl(struct queue *q)
{
	unsigned int h, t;
	void *elem;

	t = q->tail;
	if (((h = read_once(&q->head, __ATOMIC_RELAXED)) & ~1) == t) {
		/* Empty... */
		atomic_inc(&q->cons_waiting, __ATOMIC_SEQ_CST);
		wait_for_change(&q->head, h & ~1);
		wait_for_change(&q->head, (h & ~1) + 1);
		atomic_dec(&q->cons_waiting, __ATOMIC_RELAXED);
	}
	/* Grab element, then increment tail. */
	elem = read_ptr(&q->elems[(t/2) % QUEUE_ELEMS], __ATOMIC_SEQ_CST);
	store_once(&q->tail, t+2, __ATOMIC_SEQ_CST);

	if (q->prod_waiting)
		wake_producer(q);

	return elem;
}

void queue_insert(struct queue *q, void *elem)
{
	unsigned int t, h;

again:
	/* Bottom bit means someone is updating now. */
	while ((h = read_once(&q->head, __ATOMIC_RELAXED)) & 1) {
		atomic_inc(&q->prod_waiting, __ATOMIC_SEQ_CST);
		wait_for_change(&q->head, h);
		atomic_dec(&q->prod_waiting, __ATOMIC_RELAXED);
	}
	t = read_once(&q->tail, __ATOMIC_RELAXED);

	if (h == t + QUEUE_ELEMS * 2) {
		/* Full.  Wait. */
		atomic_inc(&q->prod_waiting, __ATOMIC_SEQ_CST);
		wait_for_change(&q->tail, t);
		atomic_dec(&q->prod_waiting, __ATOMIC_RELAXED);
		goto again;
	}

	/* This tells everyone we're updating. */
	if (!compare_and_swap(&q->head, h, h+1, __ATOMIC_ACQUIRE))
		goto again;

	store_ptr(&q->elems[(h/2) % QUEUE_ELEMS], elem, __ATOMIC_RELAXED);
	assert(read_once(&q->head, __ATOMIC_RELAXED) == h + 1);
	store_once(&q->head, h+2, __ATOMIC_RELEASE);

	if (read_once(&q->cons_waiting, __ATOMIC_SEQ_CST))
		wake_consumer(q);
	return;
}

void *queue_remove(struct queue *q)
{
	unsigned int h, t;
	void *elem;

	do {
		for (;;) {
			/* Read tail before head (reverse how they change) */
			t = read_once(&q->tail, __ATOMIC_SEQ_CST);
			h = read_once(&q->head, __ATOMIC_SEQ_CST);
			if ((h & ~1) != t)
				break;
			/* Empty... */
			atomic_inc(&q->cons_waiting, __ATOMIC_SEQ_CST);
			wait_for_change(&q->head, h);
			atomic_dec(&q->cons_waiting, __ATOMIC_RELAXED);
		}
		assert(t < h);
		elem = read_ptr(&q->elems[(t/2) % QUEUE_ELEMS],
				__ATOMIC_SEQ_CST);
	} while (!compare_and_swap(&q->tail, t, t+2, __ATOMIC_SEQ_CST));

	if (q->prod_waiting)
		wake_producer(q);

	return elem;
}
