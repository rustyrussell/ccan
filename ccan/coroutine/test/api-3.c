#include <stdlib.h>

#include <ccan/coroutine/coroutine.h>
#include <ccan/tap/tap.h>

/* Test metadata */
#define META_MAGIC 0x4d86aa82ec1892f6
#define BUFSIZE    8192

struct metadata {
	uint64_t magic;
};

struct state {
	struct coroutine_state ret;
	unsigned long total;
};

/* Touch a bunch of stack */
static void clobber(void *p)
{
	struct state *s = (struct state *)p;
	char buf[BUFSIZE - COROUTINE_MIN_STKSZ];
	int i;

	for (i = 0; i < sizeof(buf); i++) {
		buf[i] = random() & 0xff;
	}

	for (i = 0; i < sizeof(buf); i++) {
		s->total += buf[i];
	}

	coroutine_jump(&s->ret);
}

static void test_metadata(struct coroutine_stack *stack)
{
	struct metadata *meta;

	ok1(stack != NULL);
	ok1(coroutine_stack_check(stack, NULL) == stack);
	ok1(coroutine_stack_size(stack)
	    == BUFSIZE - COROUTINE_STK_OVERHEAD - sizeof(struct metadata));

	meta = coroutine_stack_to_metadata(stack, sizeof(*meta));
	ok1(coroutine_stack_from_metadata(meta, sizeof(*meta)) == stack);

	meta->magic = META_MAGIC;
	ok1(meta->magic == META_MAGIC);

	if (COROUTINE_AVAILABLE) {
		struct coroutine_state t;
		struct state s = {
			.total = 0,
		};

		coroutine_init(&t, clobber, &s, stack);
		coroutine_switch(&s.ret, &t);
		ok1(s.total != 0);
	} else {
		skip(1, "Coroutines not available");
	}

	ok1(coroutine_stack_to_metadata(stack, sizeof(*meta)) == meta);
	ok1(coroutine_stack_from_metadata(meta, sizeof(*meta)) == stack);
	ok1(meta->magic == META_MAGIC);
}

int main(void)
{
	char *buf;
	struct coroutine_stack *stack;

	/* This is how many tests you plan to run */
	plan_tests(1 + 2 * 9);

	/* Fix seed so we get consistent, though pseudo-random results */	
	srandom(0);

	stack = coroutine_stack_alloc(BUFSIZE, sizeof(struct metadata));
	test_metadata(stack);
	coroutine_stack_release(stack, sizeof(struct metadata));

	buf = malloc(BUFSIZE);
	ok1(buf != NULL);
	stack = coroutine_stack_init(buf, BUFSIZE, sizeof(struct metadata));
	test_metadata(stack);
	coroutine_stack_release(stack, sizeof(struct metadata));

	free(buf);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
