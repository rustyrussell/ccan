#include <ccan/lpq/lpq.h>
#include <ccan/array_size/array_size.h>
#include <ccan/permutation/permutation.h>
#include <ccan/tap/tap.h>

struct waiter {
	int intval;
	float floatval;
	struct lpq_link link;
};

static void test_array(struct waiter *waiters, int n,
		       total_order_cb(order_cb, struct waiter, ptrint_t *),
		       ptrint_t *order_ctx, bool invert)
{
	LPQ(struct waiter, link) pq;
	struct permutation *pi = permutation_new(n);
	int i;

	lpq_init(&pq, order_cb, order_ctx);

	ok1(lpq_empty(&pq));
	ok1(lpq_front(&pq) == NULL);
	ok1(lpq_dequeue(&pq) == NULL);
	ok1(lpq_empty(&pq));

	do {
		for (i = 0; i < n; i++) {
			lpq_enqueue(&pq, &waiters[pi->v[i]]);
			ok1(!lpq_empty(&pq));
			ok1(lpq_front(&pq) != NULL);
		}

		for (i = 0; i < n; i++) {
			int expected = invert ? i : (n - 1 - i);

			ok1(!lpq_empty(&pq));
			ok1(lpq_front(&pq) == &waiters[expected]);
			ok1(lpq_dequeue(&pq) == &waiters[expected]);
		}

		ok1(lpq_empty(&pq));
	} while (permutation_change(pi));
	free(pi);
}

#define ARRAY_NTESTS(arr) \
	((1 + 5 * ARRAY_SIZE(arr)) * permutation_count(ARRAY_SIZE(arr)) + 4)

static void test_reorder(void)
{
	struct waiter waiters[] = {
		{ .intval = -1, },
		{ .intval = 0, },
		{ .intval = 1, },
		{ .intval = 12, },
	};
	int n = ARRAY_SIZE(waiters);
	total_order_by_field(order, int, struct waiter, intval);
	LPQ(struct waiter, link) pq;
	int i;

	lpq_init(&pq, order.cb, order.ctx);

	for (i = 0; i < n; i++)
		lpq_enqueue(&pq, &waiters[i]);

	for (i = 0; i < n; i++) {
		waiters[i].intval = -waiters[i].intval;
		lpq_reorder(&pq, &waiters[i]);
	}

	for (i = 0; i < n; i++) {
		ok1(lpq_dequeue(&pq) == &waiters[i]);
	}

	ok1(lpq_empty(&pq));
}

int main(void)
{
	struct waiter w1[] = {
		{ .intval = -1, },
		{ .intval = 0, },
		{ .intval = 1, },
		{ .intval = 12, },
	};
	total_order_by_field(order1, int, struct waiter, intval);
	total_order_by_field(order1r, int_reverse, struct waiter, intval);
	struct waiter w2[] = {
		{ .floatval = 0.01, },
		{ .floatval = 0.1, },
		{ .floatval = 0.2 },
		{ .floatval = 1.0E+18, },
	};
	total_order_by_field(order2, float, struct waiter, floatval);
	total_order_by_field(order2r, float_reverse, struct waiter, floatval);

	/* This is how many tests you plan to run */
	plan_tests(2 * (ARRAY_NTESTS(w1) + ARRAY_NTESTS(w2)) + 5);

	test_array(w1, ARRAY_SIZE(w1), order1.cb, order1.ctx, false);
	test_array(w1, ARRAY_SIZE(w1), order1r.cb, order1r.ctx, true);

	test_array(w2, ARRAY_SIZE(w2), order2.cb, order2.ctx, false);
	test_array(w2, ARRAY_SIZE(w2), order2r.cb, order2r.ctx, true);

	test_reorder();

	/* This exits depending on whether all tests passed */
	return exit_status();
}
