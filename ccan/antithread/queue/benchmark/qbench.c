/* How long does it take to pass 100M pointers? */
#define _GNU_SOURCE
#include <sched.h>
#include <ccan/antithread/antithread.h>
/* We want access to internals. */
#include <ccan/antithread/queue/queue.c>
#include <ccan/err/err.h>
#include <ccan/time/time.h>
#include <stdio.h>

static unsigned int prod_num, cons_num;

static void bind_to_cpu(struct at_parent *parent, void *cpu)
{
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET((unsigned long)cpu, &mask);
	if (sched_setaffinity(0, sizeof(mask), &mask) != 0)
		err(1, "sched_setaffinity(%lu)", (long)cpu);
	at_tell_parent(parent, parent);
}

/* For benchmarking... */
static void queue_discard(struct queue *q)
{
	unsigned int h;

	h = read_once(&q->head, __ATOMIC_RELAXED);

	store_once(&q->tail, h, __ATOMIC_RELAXED);
}

static void queue_fill(struct queue *q)
{
	unsigned int t;

	t = read_once(&q->tail, __ATOMIC_RELAXED);

	store_once(&q->head, t + QUEUE_ELEMS*2, __ATOMIC_RELAXED);
}

static void *produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		queue_insert(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

static void *produce_no_consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		if (i % QUEUE_ELEMS == 0)
			queue_discard(q);
		queue_insert(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

/* We double index, to maintain compatibility, but otherwise this uses
 * the buf_ring(9) scheme from FreeBSD:
 *
 * Copyright (c) 2007-2009 Kip Macy <kmacy@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
static void bsd_queue_insert(struct queue *q, void *elem)
{
	unsigned int prod_head;
	unsigned int cons_tail;

	do {
		prod_head = read_once(&q->prod_lock, __ATOMIC_RELAXED);
		cons_tail = read_once(&q->tail, __ATOMIC_RELAXED);

		if (prod_head == cons_tail + QUEUE_ELEMS*2) {
			/* Full.  Wait. */
			atomic_inc(&q->prod_waiting, __ATOMIC_SEQ_CST);
			wait_for_change(&q->tail, cons_tail);
			atomic_dec(&q->prod_waiting, __ATOMIC_RELAXED);
			continue;
		}
	} while (!compare_and_swap(&q->prod_lock, prod_head, prod_head + 2,
				   __ATOMIC_SEQ_CST));
	store_ptr(&q->elems[(prod_head / 2) % QUEUE_ELEMS],
		  elem, __ATOMIC_SEQ_CST);

	/*
	 * If there are other enqueues in progress
	 * that preceeded us, we need to wait for them
	 * to complete
	 */
	while (read_once(&q->head, __ATOMIC_SEQ_CST) != prod_head);

	store_once(&q->head, prod_head + 2, __ATOMIC_SEQ_CST);
	if (read_once(&q->cons_waiting, __ATOMIC_SEQ_CST))
		wake_consumer(q);
}

static void *bsd_produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		bsd_queue_insert(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

static void *bsd_produce_no_consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		if (i % QUEUE_ELEMS == 0)
			queue_discard(q);
		bsd_queue_insert(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}
static void *consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++)
		ret -= (unsigned long)queue_remove(q);
	return (void *)ret;
}

static void *consume_no_produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++) {
		if (i % QUEUE_ELEMS == 0)
			queue_fill(q);
		ret -= (unsigned long)queue_remove(q);
	}
	return (void *)ret;
}

static void *produce_excl(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
		queue_insert_excl(q, (void *)i);
		sum += i;
	}
	return (void *)sum;
}

static void *consume_excl(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++)
		ret -= (unsigned long)queue_remove_excl(q);
	return (void *)ret;
}

static void lock(unsigned int *lptr)
{
	while (__atomic_test_and_set(lptr, __ATOMIC_ACQUIRE));
}

static void unlock(unsigned int *lptr)
{
	__atomic_clear(lptr, __ATOMIC_RELEASE);
}

static void *dumb_produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail + QUEUE_ELEMS) {
			unlock(&q->prod_lock);
			while (q->head == __atomic_load_n(&q->tail, __ATOMIC_RELAXED) + QUEUE_ELEMS);
			goto again;
		}
		q->elems[q->head % QUEUE_ELEMS] = (void *)i;
		q->head++;
		unlock(&q->prod_lock);
		sum += i;
	}

	return (void *)sum;
}

static void *dumb_produce_no_consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, sum = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < prod_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail + QUEUE_ELEMS) {
			queue_discard(q);
			unlock(&q->prod_lock);
			goto again;
		}
		q->elems[q->head % QUEUE_ELEMS] = (void *)i;
		q->head++;
		unlock(&q->prod_lock);
		sum += i;
	}

	return (void *)sum;
}

static void *dumb_consume(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail) {
			unlock(&q->prod_lock);
			while (q->tail == __atomic_load_n(&q->head, __ATOMIC_RELAXED));
			goto again;
		}
		ret -= (unsigned long)q->elems[q->tail % QUEUE_ELEMS];
		q->tail++;
		unlock(&q->prod_lock);
	}

	return (void *)ret;
}

static void *dumb_consume_no_produce(struct at_parent *parent, void *cpu)
{
	unsigned long i, ret = 0;
	struct queue *q;

	bind_to_cpu(parent, cpu);

	q = at_read_parent(parent);

	for (i = 0; i < cons_num; i++) {
	again:
		lock(&q->prod_lock);
		if (q->head == q->tail) {
			queue_fill(q);
			unlock(&q->prod_lock);
			goto again;
		}
		ret -= (unsigned long)q->elems[q->tail % QUEUE_ELEMS];
		q->tail++;
		unlock(&q->prod_lock);
	}

	return (void *)ret;
}


int main(int argc, char *argv[])
{
	struct at_pool *pool;
	struct queue *q;
	unsigned long i, num, prods, cons, total;
	struct timespec start, end;
	char numstr[50];
	struct at_child **children;
	void *(*consfn)(struct at_parent *parent, void *cpu) = consume;
	void *(*prodfn)(struct at_parent *parent, void *cpu) = produce;
	char **orig_args = argv;

	if (argv[1] && strcmp(argv[1], "--xprod") == 0) {
		prodfn = produce_excl;
		argv++;
		argc--;
	}
	if (argv[1] && strcmp(argv[1], "--xcons") == 0) {
		consfn = consume_excl;
		argv++;
		argc--;
	}
	if (argv[1] && strcmp(argv[1], "--dumb") == 0) {
		consfn = dumb_consume;
		prodfn = dumb_produce;
		argv++;
		argc--;
	}
	if (argv[1] && strcmp(argv[1], "--bsd") == 0) {
		prodfn = bsd_produce;
		argv++;
		argc--;
	}

	if (argc != 4)
		errx(1, "Usage: qbench [--xprod][--xcons][--dumb][--bsd] <num> <num-producers> <num-consumers>");

	num = atoi(argv[1]);
	prods = atoi(argv[2]);
	cons = atoi(argv[3]);

	if (prods == 0) {
		if (consfn == dumb_consume)
			consfn = dumb_consume_no_produce;
		else
			consfn = consume_no_produce;
	}
	if (cons == 0) {
		if (prodfn == dumb_produce)
			prodfn = dumb_produce_no_consume;
		else if (prodfn == bsd_produce)
			prodfn = bsd_produce_no_consume;
			prodfn = produce_no_consume;
	}

	/* Make it evenly divisible. */
	if (prods && cons) {
		while ((num % prods) || (num % cons))
			num++;
	}
	sprintf(numstr, "%lu", num);
	argv[1] = numstr;

	if (prods)
		prod_num = num / prods;
	if (cons)
		cons_num = num / cons;

	children = tal_arr(NULL, struct at_child *, prods + cons);

	pool = at_new_pool(sizeof(*q));
	q = tal(pool, struct queue);
	queue_init(q);

	for (i = 0; i < prods + cons; i++) {
		children[i] = at_run(pool, i < prods ? prodfn : consfn,
				     (void *)i);
		if (!at_read_child(children[i]))
			err(1, "Child %li failed", i);
	}

	for (i = 0; i < prods + cons; i++)
		at_tell_child(children[i], q);

	start = time_now();
	for (i = total = 0; i < prods + cons; i++) {
		void *ret = at_read_child(children[i]);
		if (!ret)
			err(1, "Child %li failed", i);
		total += (unsigned long)ret;
	}
	end = time_now();

	/* They should cancel out... */
	if (prods && cons && total != 0)
		errx(1, "Unbalanced total: %li\n", total);

	/* Kill them all. */
	tal_free(pool);
	while (*orig_args) {
		printf("%s ", *orig_args);
		orig_args++;
	}
	printf("in %llums\n", (long long)time_to_msec(time_sub(end, start)));
	return 0;
}
