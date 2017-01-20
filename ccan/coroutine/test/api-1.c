#include <stdlib.h>

#include <ccan/coroutine/coroutine.h>
#include <ccan/tap/tap.h>

#define STKSZ		(COROUTINE_MIN_STKSZ + COROUTINE_STK_OVERHEAD)

static int global = 0;

static void trivial_fn(void *p)
{
	struct coroutine_state *ret = (struct coroutine_state *)p;

	global = 1;

	coroutine_jump(ret);
}

static void test_trivial(struct coroutine_stack *stack)
{
	struct coroutine_state t, master;

	ok1(stack != NULL);
	ok1(coroutine_stack_check(stack, NULL) == stack);
	ok1(coroutine_stack_size(stack) == COROUTINE_MIN_STKSZ);

	if (!COROUTINE_AVAILABLE) {
		skip(1, "Coroutines not available");
		return;
	}

	coroutine_init(&t, trivial_fn, &master, stack);
	coroutine_switch(&master, &t);

	ok1(global == 1);
}


static char buf[STKSZ];

int main(void)
{
	struct coroutine_stack *stack;

	/* This is how many tests you plan to run */
	plan_tests(2 * 4 + 1);

	stack = coroutine_stack_init(buf, sizeof(buf), 0);
	test_trivial(stack);
	coroutine_stack_release(stack, 0);
	ok1(!coroutine_stack_check(stack, NULL));

	stack = coroutine_stack_alloc(STKSZ, 0);
	test_trivial(stack);
	coroutine_stack_release(stack, 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
