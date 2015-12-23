#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <ccan/deque/deque.h>
/* Include the C files directly. */
#include <ccan/deque/deque.c>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	failtest_init(argc, argv);
	plan_tests(18);

	DEQ_WRAP(char) *a;
	ok1(deq_new(a, 2, DEQ_SHRINK_IF_EMPTY) == 0); // two mallocs
	ok1(a && deq_push(a, 'a') == 1);
	ok1(a && deq_push(a, 'b') == 1);
	ok1(a && deq_push(a, 'c') == 1); // malloc to grow

	char t;
	ok1(a && deq_pop(a, &t) == 1);
	ok1(a && deq_pop(a, &t) == 1);
	ok1(a && deq_pop(a, &t) == 1);   // malloc to shrink

	if (a) deq_free(a);

	DEQ_WRAP(char) *b;
	ok1(deq_new(b, 5, DEQ_SHRINK_AT_20PCT) == 0); // two mallocs
	int i;
	for (i = 0; b && i < 6; i++)
		ok1(deq_push(b, i + 'A') == 1); // last iteration mallocs to grow
	for (; b && i > 2; i--)
		ok1(deq_pop(b, &t) == 1);       // last iteration mallocs to shrink

	if (b) deq_free(b);

	failtest_exit(exit_status());
}
