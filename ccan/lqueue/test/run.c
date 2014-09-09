#include "config.h"

#include <ccan/lqueue/lqueue.h>
#include <ccan/tap/tap.h>

struct waiter {
	const char *name;
	struct lqueue_link ql;
};

int main(void)
{
	LQUEUE(q);
	struct waiter a = { "Alice" };
	struct waiter b = { "Bob" };
	struct waiter c = { "Carol" };
	struct waiter *waiter;

	/* This is how many tests you plan to run */
	plan_tests(25);

	ok1(lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == NULL);
	ok1(lqueue_back(&q, struct waiter, ql) == NULL);

	lqueue_enqueue(&q, &a, ql);

	ok1(!lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == &a);
	ok1(lqueue_back(&q, struct waiter, ql) == &a);

	lqueue_enqueue(&q, &b, ql);

	ok1(!lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == &a);
	ok1(lqueue_back(&q, struct waiter, ql) == &b);

	lqueue_enqueue(&q, &c, ql);

	ok1(!lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == &a);
	ok1(lqueue_back(&q, struct waiter, ql) == &c);

	waiter = lqueue_dequeue(&q, struct waiter, ql);
	ok1(waiter == &a);

	ok1(!lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == &b);
	ok1(lqueue_back(&q, struct waiter, ql) == &c);

	waiter = lqueue_dequeue(&q, struct waiter, ql);
	ok1(waiter == &b);

	ok1(!lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == &c);
	ok1(lqueue_back(&q, struct waiter, ql) == &c);

	waiter = lqueue_dequeue(&q, struct waiter, ql);
	ok1(waiter == &c);

	ok1(lqueue_empty(&q));
	ok1(lqueue_front(&q, struct waiter, ql) == NULL);
	ok1(lqueue_back(&q, struct waiter, ql) == NULL);

	ok1(lqueue_dequeue(&q, struct waiter, ql) == NULL);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
