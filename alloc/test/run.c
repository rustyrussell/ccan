#include "alloc/alloc.h"
#include "tap.h"
#include "alloc/alloc.c"
#include <stdlib.h>

#define POOL_ORD 16
#define POOL_SIZE (1 << POOL_ORD)

#define sort(p, num, cmp) \
	qsort((p), (num), sizeof(*p), (int(*)(const void *, const void *))cmp)

static int addr_cmp(void **a, void **b)
{
	return (*a) - (*b);
}

static bool unique(void *p[], unsigned int num)
{
	unsigned int i;

	for (i = 1; i < num; i++)
		if (p[i] == p[i-1])
			return false;
	return true;
}	

int main(int argc, char *argv[])
{
	void *mem;
	unsigned int i, num, max_size;
	void *p[POOL_SIZE];

	plan_tests(141);

	/* FIXME: Needs to be page aligned for now. */
	posix_memalign(&mem, getpagesize(), POOL_SIZE);

	/* Small pool, all allocs fail, even 0-length. */
	alloc_init(mem, 0);
	ok1(alloc_check(mem, 0));
	ok1(alloc_get(mem, 0, 1, 1) == NULL);
	ok1(alloc_get(mem, 0, 128, 1) == NULL);
	ok1(alloc_get(mem, 0, 0, 1) == NULL);

	/* Free of NULL should work. */
	alloc_free(mem, 0, NULL);

	alloc_init(mem, POOL_SIZE);
	ok1(alloc_check(mem, POOL_SIZE));
	/* Find largest allocation which works. */
	for (max_size = POOL_SIZE * 2; max_size; max_size--) {
		p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
		if (p[0])
			break;
	}
	ok1(max_size < POOL_SIZE);
	ok1(max_size > 0);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Free it, should be able to reallocate it. */
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_check(mem, POOL_SIZE));
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Allocate a whole heap. */
	for (i = 0; i < POOL_SIZE; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, 1, 1);
		if (!p[i])
			break;
	}

	num = i;
	/* Can't allocate this many. */
	ok1(num != POOL_SIZE);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Sort them. */
	sort(p, num, addr_cmp);

	/* Uniqueness check */
	ok1(unique(p, num));

	/* Free every second one. */
	for (i = 0; i < num; i += 2) {
		alloc_free(mem, POOL_SIZE, p[i]);
		ok1(alloc_check(mem, POOL_SIZE));
	}
	for (i = 1; i < num; i += 2) {
		alloc_free(mem, POOL_SIZE, p[i]);
		ok1(alloc_check(mem, POOL_SIZE));
	}
	ok1(alloc_check(mem, POOL_SIZE));

	/* Should be able to reallocate max size. */
	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Re-initializing should be the same as freeing everything */
	alloc_init(mem, POOL_SIZE);
	ok1(alloc_check(mem, POOL_SIZE));
	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_check(mem, POOL_SIZE));
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Alignment constraints should be met, as long as powers of two */
	for (i = 0; i < POOL_ORD-2 /* FIXME: Should be -1 */; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, i, 1 << i);
		ok1(p[i]);
		ok1(((unsigned long)p[i] % (1 << i)) == 0);
		ok1(alloc_check(mem, POOL_SIZE));
	}

	for (i = 0; i < POOL_ORD-2 /* FIXME: Should be -1 */; i++) {
		alloc_free(mem, POOL_SIZE, p[i]);
		ok1(alloc_check(mem, POOL_SIZE));
	}

	/* Alignment constraints for a single-byte allocation. */
	for (i = 0; i < POOL_ORD; i++) {
		p[0] = alloc_get(mem, POOL_SIZE, 1, 1 << i);
		ok1(p[0]);
		ok1(alloc_check(mem, POOL_SIZE));
		alloc_free(mem, POOL_SIZE, p[0]);
		ok1(alloc_check(mem, POOL_SIZE));
	}

	return exit_status();
}
