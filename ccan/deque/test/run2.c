#include <stdlib.h>
#include <ccan/tap/tap.h>
#include <ccan/deque/deque.h>
/* Include the C files directly. */

size_t malloc_sz;
#define malloc(x) malloc(malloc_sz = x)
#include <ccan/deque/deque.c>

int main(void)
{
	struct quad { int w, x, y, z; } p, q, r, s, *save;
	assert(sizeof(struct quad) == sizeof(int) * 4);

	plan_tests(20);

	typedef DEQ_WRAP(struct quad) qd_t;
	qd_t a_, *a = &a_;

	ok1(deq_init(a, 4, DEQ_SHRINK_AT_20PCT) == 0);
	ok1(malloc_sz == sizeof(struct quad) * 4);

	ok1(a->v && a->deq.head == 0 && a->deq.tail == 0 && a->deq.len == 0 && a->deq.cap == 4 &&
	    a->deq.min == 4 && a->deq.esz == sizeof(struct quad) && a->deq.shrink == DEQ_SHRINK_AT_20PCT);
	save = a->v;
	memset(a->v, 0xFF, a->deq.cap * sizeof(struct quad));

	int chk[12] = { 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3 };
	p = (struct quad) { 1, 1, 1, 1 };
	ok1(deq_push(a, p) == 1);
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 1 && a->deq.len == 1 && a->deq.cap == 4);
	ok1(memcmp(a->v, chk, sizeof(int) * 4) == 0 && memcmp(a->v, chk, sizeof(chk)) != 0);
	q = (struct quad) { 2, 2, 2, 2 };
	ok1(deq_push(a, q) == 1);
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 2 && a->deq.len == 2 && a->deq.cap == 4);
	ok1(memcmp(a->v, chk, sizeof(int) * 8) == 0 && memcmp(a->v, chk, sizeof(chk)) != 0);
	r = (struct quad) { 3, 3, 3, 3 };
	ok1(deq_push(a, r) == 1);
	ok1(a->v == save && a->deq.head == 0 && a->deq.tail == 3 && a->deq.len == 3 && a->deq.cap == 4);
	ok1(memcmp(a->v, chk, sizeof(int) * 12) == 0);

	ok1(deq_shift(a, &s) == 1 && s.w == 1 && s.x == 1 && s.y == 1 && s.z == 1);
	ok1(deq_shift(a, &s) == 1 && s.w == 2 && s.x == 2 && s.y == 2 && s.z == 2);
	ok1(deq_shift(a, &s) == 1 && s.w == 3 && s.x == 3 && s.y == 3 && s.z == 3);
	ok1(a->v == save && a->deq.head == 3 && a->deq.tail == 3 && a->deq.len == 0 && a->deq.cap == 4);
	deq_push(a, p);
	deq_push(a, q);
	deq_push(a, r);
	deq_push(a, p);
	ok1(a->v == save && a->deq.head == 3 && a->deq.tail == 3 && a->deq.len == 4 && a->deq.cap == 4);

	deq_push(a, q);
	ok1(a->v != save && a->deq.head == 0 && a->deq.tail == 5 && a->deq.len == 5 && a->deq.cap == 8);
	ok1(malloc_sz == sizeof(struct quad) * 8);
	save = a->v;
	deq_unshift(a, r);
	ok1(a->v == save && a->deq.head == 7 && a->deq.tail == 5 && a->deq.len == 6 && a->deq.cap == 8);

	deq_reset(a);

	return exit_status();
}
