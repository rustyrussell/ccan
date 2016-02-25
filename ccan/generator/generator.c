/* Licensed LGPLv2.1+ - see LICENSE file for details */
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>

#include <ccan/alignof/alignof.h>

#include <ccan/generator/generator.h>

#define DEFAULT_STATE_SIZE	8192
#define STATE_ALIGN		ALIGNOF(struct generator_)

void *generator_new_(generator_wrapper_ *fn, size_t retsize)
{
	char *base;
	size_t size = DEFAULT_STATE_SIZE;
	void *ret;
	struct generator_ *gen;

	base = malloc(size);
	if (!base)
		abort();

	retsize = (retsize + STATE_ALIGN) & ~(STATE_ALIGN - 1);
	ret = base + size - retsize;
	gen = (struct generator_ *)ret - 1;

	gen->base = base;
	gen->complete = false;

	getcontext(&gen->gen);

	gen->gen.uc_stack.ss_sp = gen->base;
	gen->gen.uc_stack.ss_size = (char *)gen - base;

	if (HAVE_POINTER_SAFE_MAKECONTEXT) {
		makecontext(&gen->gen, (void *)fn, 1, ret);
	} else {
		ptrdiff_t si = ptr2int(ret);
		ptrdiff_t mask = (1UL << (sizeof(int) * 8)) - 1;
		int lo = si & mask;
		int hi = si >> (sizeof(int) * 8);

		makecontext(&gen->gen, (void *)fn, 2, lo, hi);
	}

	return ret;
}

void generator_free_(void *ret)
{
	struct generator_ *gen = generator_state_(ret);
	free(gen->base);
}
