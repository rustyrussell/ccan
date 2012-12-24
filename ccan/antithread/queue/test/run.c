#include <ccan/antithread/antithread.h>
#include <ccan/antithread/queue/queue.h>
#include <ccan/antithread/queue/queue.c>
#include <ccan/tap/tap.h>
#include <stdio.h>

#define LIMIT ((char *)10000L)

static void *enqueue(struct at_parent *parent, struct queue *q)
{
	char *i;

	for (i = NULL; i < LIMIT; i++)
		queue_insert(q, i);
	return i - 1;
}

static void *dequeue(struct at_parent *parent, struct queue *q)
{
	char *i, *p;

	for (i = NULL; i < LIMIT; i++) {
		p = queue_remove(q);
		if (p != i) {
			fprintf(stderr, "Expected %p, got %p!\n", i, p);
			break;
		}
	}
	return p;
}

static void *enqueue_excl(struct at_parent *parent, struct queue *q)
{
	char *i;

	for (i = NULL; i < LIMIT; i++)
		queue_insert_excl(q, i);
	return i - 1;
}

static void *dequeue_excl(struct at_parent *parent, struct queue *q)
{
	char *i, *p;

	for (i = NULL; i < LIMIT; i++) {
		p = queue_remove_excl(q);
		if (p != i) {
			fprintf(stderr, "Expected %p, got %p!\n", i, p);
			break;
		}
	}
	return p;
}

int main(void)
{
	struct queue *q;
	char *e, *d;
	struct at_pool *pool;
	struct at_child *enqueue_child, *dequeue_child;

	plan_tests(12);

	pool = at_new_pool(sizeof(*q));
	ok1(pool);
	q = tal(pool, struct queue);
	ok1(q);
	queue_init(q);

	/* Exclusive insert / remove. */
	queue_insert_excl(q, q);
	ok1(queue_remove_excl(q) == q);
	queue_insert_excl(q, q+1);
	queue_insert_excl(q, q+2);
	ok1(queue_remove_excl(q) == q+1);
	ok1(queue_remove_excl(q) == q+2);

	/* Non-exclusive insert / remove. */
	queue_insert(q, q);
	ok1(queue_remove(q) == q);
	queue_insert(q, q+1);
	queue_insert(q, q+2);
	ok1(queue_remove(q) == q+1);
	ok1(queue_remove(q) == q+2);
	fflush(stdout);

	/* Exclusive variants, reader and writer in parallel. */
	dequeue_child = at_run(pool, dequeue_excl, q);
	enqueue_child = at_run(pool, enqueue_excl, q);

	e = at_read_child(enqueue_child);
	d = at_read_child(dequeue_child);
	ok1(e == LIMIT-1);
	ok1(d == LIMIT-1);
	tal_free(enqueue_child);
	tal_free(dequeue_child);
	fflush(stdout);

	/* Non-exclusive variants, reader and writer in parallel. */
	dequeue_child = at_run(pool, dequeue, q);
	enqueue_child = at_run(pool, enqueue, q);

	e = at_read_child(enqueue_child);
	d = at_read_child(dequeue_child);
	ok1(e == LIMIT-1);
	ok1(d == LIMIT-1);

	tal_free(enqueue_child);
	tal_free(dequeue_child);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
