#include "config.h"

#include <ccan/lstack/lstack.h>
#include <ccan/tap/tap.h>

struct stacker {
	const char *name;
	struct lstack_link sl;
};

int main(void)
{
	LSTACK(struct stacker, sl) s = LSTACK_INIT;
	struct stacker a = { "Alice" };
	struct stacker b = { "Bob" };
	struct stacker c = { "Carol" };
	struct stacker *stacker;

	/* This is how many tests you plan to run */
	plan_tests(18);

	ok1(lstack_empty(&s));
	diag("top = %p\n", lstack_top(&s));
	ok1(lstack_top(&s) == NULL);

	lstack_push(&s, &a);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s) == &a);

	lstack_push(&s, &b);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s) == &b);

	lstack_push(&s, &c);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s) == &c);

	stacker = lstack_pop(&s);
	ok1(stacker == &c);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s) == &b);

	stacker = lstack_pop(&s);
	ok1(stacker == &b);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s) == &a);

	stacker = lstack_pop(&s);
	ok1(stacker == &a);

	ok1(lstack_empty(&s));
	ok1(lstack_top(&s) == NULL);

	ok1(lstack_pop(&s) == NULL);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
