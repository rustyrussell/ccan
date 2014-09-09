#include "config.h"

#include <ccan/lstack/lstack.h>
#include <ccan/tap/tap.h>

struct stacker {
	const char *name;
	struct lstack_link sl;
};

int main(void)
{
	LSTACK(s);
	struct stacker a = { "Alice" };
	struct stacker b = { "Bob" };
	struct stacker c = { "Carol" };
	struct stacker *stacker;

	/* This is how many tests you plan to run */
	plan_tests(18);

	ok1(lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == NULL);

	lstack_push(&s, &a, sl);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == &a);

	lstack_push(&s, &b, sl);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == &b);

	lstack_push(&s, &c, sl);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == &c);

	stacker = lstack_pop(&s, struct stacker, sl);
	ok1(stacker == &c);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == &b);

	stacker = lstack_pop(&s, struct stacker, sl);
	ok1(stacker == &b);

	ok1(!lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == &a);

	stacker = lstack_pop(&s, struct stacker, sl);
	ok1(stacker == &a);

	ok1(lstack_empty(&s));
	ok1(lstack_top(&s, struct stacker, sl) == NULL);

	ok1(lstack_pop(&s, struct stacker, sl) == NULL);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
