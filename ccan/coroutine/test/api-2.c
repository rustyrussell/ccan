#include <stdlib.h>

#include <ccan/coroutine/coroutine.h>
#include <ccan/tap/tap.h>

struct state {
	struct coroutine_state c1, c2;
	struct coroutine_state master;
	int val;
};

static void f1(void *p)
{
	struct state *state = (struct state *)p;

	coroutine_switch(&state->c1, &state->c2);

	ok(state->val == 17, "state->val == %d [expected 17]", state->val);
	state->val = 23;

	coroutine_switch(&state->c1, &state->c2);

	ok(state->val == 24, "state->val == %d [expected 24]", state->val);

	coroutine_switch(&state->c1, &state->c2);

	ok(state->val == 26, "state->val == %d [expected 26]", state->val);

	coroutine_switch(&state->c1, &state->c2);

	ok(state->val == 29, "state->val == %d [expected 29]", state->val);

	coroutine_switch(&state->c1, &state->c2);
}

static void f2(void *p)
{
	struct state *state = (struct state *)p;

	state->val = 17;

	coroutine_switch(&state->c2, &state->c1);

	ok(state->val == 23, "state->val == %d [expected 23]", state->val);
	state->val += 1;

	coroutine_switch(&state->c2, &state->c1);

	state->val += 2;

	coroutine_switch(&state->c2, &state->c1);

	state->val += 3;

	coroutine_switch(&state->c2, &state->c1);

	coroutine_jump(&state->master);
}

static void test1(size_t bufsz)
{
	void *buf1, *buf2;
	struct coroutine_stack *stack1, *stack2;

	buf1 = malloc(bufsz);
	ok1(buf1 != NULL);
	stack1 = coroutine_stack_init(buf1, bufsz, 0);
	diag("buf1=%p stack1=%p bufsz=0x%zx overhead=0x%zx\n",
	     buf1, stack1, bufsz, COROUTINE_STK_OVERHEAD);
	ok1(coroutine_stack_check(stack1, NULL) == stack1);
	ok1(coroutine_stack_size(stack1) == bufsz - COROUTINE_STK_OVERHEAD);

	buf2 = malloc(bufsz);
	ok1(buf2 != NULL);
	stack2 = coroutine_stack_init(buf2, bufsz, 0);
	ok1(coroutine_stack_check(stack2, NULL) == stack2);
	ok1(coroutine_stack_size(stack2) == bufsz - COROUTINE_STK_OVERHEAD);

	if (COROUTINE_AVAILABLE) {
		struct state s;

		coroutine_init(&s.c1, f1, &s, stack1);
		coroutine_init(&s.c2, f2, &s, stack2);

		coroutine_switch(&s.master, &s.c1);
	} else {
		skip(5, "Coroutines not available");
	}

	ok(1, "Completed test1");

	coroutine_stack_release(stack1, 0);
	ok1(coroutine_stack_check(stack1, NULL) == NULL);
	free(buf1);
	coroutine_stack_release(stack2, 0);
	ok1(coroutine_stack_check(stack2, NULL) == NULL);
	free(buf2);
}


int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(14);

	test1(8192);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
