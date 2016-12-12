/* Licensed LGPLv2.1+ - see LICENSE file for details */
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include <ccan/alignof/alignof.h>

#include <ccan/generator/generator.h>

#define DEFAULT_STATE_SIZE	8192
#define STATE_ALIGN		ALIGNOF(struct generator_)

static size_t generator_metasize(size_t retsize)
{
	retsize = (retsize + STATE_ALIGN) & ~(STATE_ALIGN - 1);
	return sizeof(struct generator_) + retsize;
}

void *generator_new_(void (*fn)(void *), size_t retsize)
{
	char *base;
	size_t size = DEFAULT_STATE_SIZE;
	size_t metasize = generator_metasize(retsize);
	struct coroutine_stack *stack;
	void *ret;
	struct generator_ *gen;

	base = malloc(size);
	if (!base)
		abort();

	retsize = (retsize + STATE_ALIGN) & ~(STATE_ALIGN - 1);

	stack = coroutine_stack_init(base, size, metasize);
	gen = coroutine_stack_to_metadata(stack, metasize);
	ret = gen + 1;

	gen->base = base;
	gen->complete = false;

	coroutine_init(&gen->gen, fn, ret, stack);

	return ret;
}

void generator_free_(void *ret, size_t retsize)
{
	struct generator_ *gen = generator_state_(ret);
	size_t metasize = generator_metasize(retsize);
	struct coroutine_stack *stack;

	stack = coroutine_stack_from_metadata(gen, metasize);
	coroutine_stack_release(stack, metasize);
	free(gen->base);
}
