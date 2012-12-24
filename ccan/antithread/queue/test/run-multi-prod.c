#include <ccan/antithread/antithread.h>
#include <ccan/antithread/queue/queue.h>
#include <ccan/antithread/queue/queue.c>
#include <ccan/tap/tap.h>
#include <stdio.h>

#define LIMIT 10000L
#define LIMIT_PTR ((char *)LIMIT)
#define NUM_ENQUEUERS 10

static void *enqueue(struct at_parent *parent, struct queue *q)
{
	char *i;

	for (i = NULL; i < LIMIT_PTR; i++)
		queue_insert(q, i);
	return q;
}

static void *dequeue(struct at_parent *parent, struct queue *q)
{
	char *p;
	unsigned int i, *counters;

	counters = tal_arrz(parent, unsigned int, LIMIT);
	for (i = 0; i < LIMIT * NUM_ENQUEUERS; i++) {
		p = queue_remove(q);
		if (p >= LIMIT_PTR) {
			diag("dequeued bad pointer %p", p);
			return NULL;
		}
		counters[(long)p]++;
	}
	return counters;
}

int main(void)
{
	struct queue *q;
	unsigned int i, *counters;
	struct at_pool *pool;
	struct at_child *enqueue_child[NUM_ENQUEUERS], *dequeue_child;

	plan_tests(4 + NUM_ENQUEUERS);

	pool = at_new_pool(LIMIT * sizeof(unsigned int));
	ok1(pool);
	q = tal(pool, struct queue);
	ok1(q);
	queue_init(q);

	/* Start enqueuers. */
	for (i = 0; i < NUM_ENQUEUERS; i++)
		enqueue_child[i] = at_run(pool, enqueue, q);

	dequeue_child = at_run(pool, dequeue, q);

	for (i = 0; i < NUM_ENQUEUERS; i++) {
		ok1(at_read_child(enqueue_child[i]) == q);
		tal_free(enqueue_child[i]);
	}

	counters = at_read_child(dequeue_child);
	ok1(counters);

	for (i = 0; i < LIMIT; i++) {
		if (counters[i] != NUM_ENQUEUERS) {
			fail("counters[%u] = %u", i, counters[i]);
			break;
		}
	}
	if (i == LIMIT)
		pass("All counters correct");

	tal_free(pool);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
