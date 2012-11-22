#include <ccan/antithread/alloc/alloc.h>
#include <ccan/tap/tap.h>
#include <ccan/antithread/alloc/alloc.c>
#include <ccan/antithread/alloc/bitops.c>
#include <ccan/antithread/alloc/tiny.c>
#include <stdlib.h>
#include <err.h>

#define sort(p, num, cmp) \
	qsort((p), (num), sizeof(*p), (int(*)(const void *, const void *))cmp)

static int addr_cmp(void **a, void **b)
{
	return (char *)(*a) - (char *)(*b);
}

static bool unique(void *p[], unsigned int num)
{
	unsigned int i;

	for (i = 1; i < num; i++)
		if (p[i] == p[i-1])
			return false;
	return true;
}

static bool free_every_second_one(void *mem, unsigned int num,
				  unsigned long pool_size, void *p[])
{
	unsigned int i;

	/* Free every second one. */
	for (i = 0; i < num; i += 2) {
		alloc_free(mem, pool_size, p[i]);
	}
	if (!alloc_check(mem, pool_size))
		return false;
	for (i = 1; i < num; i += 2) {
		alloc_free(mem, pool_size, p[i]);
	}
	if (!alloc_check(mem, pool_size))
		return false;
	return true;
}

static void test(unsigned int pool_size)
{
	void *mem;
	unsigned int i, num, max_size;
	void **p = calloc(pool_size, sizeof(*p));
	unsigned alloc_limit = pool_size / 2;

	mem = malloc(pool_size);

	/* Small pool, all allocs fail, even 0-length. */
	alloc_init(mem, 0);
	ok1(alloc_check(mem, 0));
	ok1(alloc_get(mem, 0, 1, 1) == NULL);
	ok1(alloc_get(mem, 0, 128, 1) == NULL);
	ok1(alloc_get(mem, 0, 0, 1) == NULL);

	/* Free of NULL should work. */
	alloc_free(mem, 0, NULL);

	alloc_init(mem, pool_size);
	ok1(alloc_check(mem, pool_size));
	/* Find largest allocation which works. */
	for (max_size = pool_size + 1; max_size; max_size--) {
		p[0] = alloc_get(mem, pool_size, max_size, 1);
		if (p[0])
			break;
	}
	ok1(max_size < pool_size);
	ok1(max_size > 0);
	ok1(alloc_check(mem, pool_size));
	ok1(alloc_size(mem, pool_size, p[0]) >= max_size);

	/* Free it, should be able to reallocate it. */
	alloc_free(mem, pool_size, p[0]);
	ok1(alloc_check(mem, pool_size));

	p[0] = alloc_get(mem, pool_size, max_size, 1);
	ok1(p[0]);
	ok1(alloc_size(mem, pool_size, p[0]) >= max_size);
	ok1(alloc_check(mem, pool_size));
	alloc_free(mem, pool_size, p[0]);
	ok1(alloc_check(mem, pool_size));

	/* Allocate a whole heap. */
	for (i = 0; i < pool_size; i++) {
		p[i] = alloc_get(mem, pool_size, 1, 1);
		if (!p[i])
			break;
	}

	/* Uncomment this for a more intuitive view of what the
	 * allocator looks like after all these 1 byte allocs. */
#if 0
	alloc_visualize(stderr, mem, pool_size);
#endif

	num = i;
	/* Can't allocate this many. */
	ok1(num != pool_size);
	ok1(alloc_check(mem, pool_size));

	/* Sort them. */
	sort(p, num, addr_cmp);

	/* Uniqueness check */
	ok1(unique(p, num));

	ok1(free_every_second_one(mem, num, pool_size, p));
	ok1(alloc_check(mem, pool_size));

	/* Should be able to reallocate max size. */
	p[0] = alloc_get(mem, pool_size, max_size, 1);
	ok1(p[0]);
	ok1(alloc_check(mem, pool_size));
	ok1(alloc_size(mem, pool_size, p[0]) >= max_size);

	/* Re-initializing should be the same as freeing everything */
	alloc_init(mem, pool_size);
	ok1(alloc_check(mem, pool_size));
	p[0] = alloc_get(mem, pool_size, max_size, 1);
	ok1(p[0]);
	ok1(alloc_size(mem, pool_size, p[0]) >= max_size);
	ok1(alloc_check(mem, pool_size));
	alloc_free(mem, pool_size, p[0]);
	ok1(alloc_check(mem, pool_size));

	/* Alignment constraints should be met, as long as powers of two */
	for (i = 0; (1 << i) < alloc_limit; i++) {
		p[i] = alloc_get(mem, pool_size, i, 1 << i);
		ok1(p[i]);
		ok1(((char *)p[i] - (char *)mem) % (1 << i) == 0);
		ok1(alloc_check(mem, pool_size));
		ok1(alloc_size(mem, pool_size, p[i]) >= i);
	}

	for (i = 0; (1 << i) < alloc_limit; i++) {
		alloc_free(mem, pool_size, p[i]);
		ok1(alloc_check(mem, pool_size));
	}

	/* Alignment constraints for a single-byte allocation. */
	for (i = 0; (1 << i) < alloc_limit; i++) {
		p[0] = alloc_get(mem, pool_size, 1, 1 << i);
		ok1(p[0]);
		ok1(((char *)p[0] - (char *)mem) % (1 << i) == 0);
		ok1(alloc_check(mem, pool_size));
		ok1(alloc_size(mem, pool_size, p[0]) >= 1);
		alloc_free(mem, pool_size, p[0]);
		ok1(alloc_check(mem, pool_size));
	}

	/* Alignment check for a 0-byte allocation.  Corner case. */
	p[0] = alloc_get(mem, pool_size, 0, alloc_limit);
	ok1(alloc_check(mem, pool_size));
	ok1(alloc_size(mem, pool_size, p[0]) < pool_size);
	alloc_free(mem, pool_size, p[0]);
	ok1(alloc_check(mem, pool_size));

	free(mem);
	free(p);
}

int main(int argc, char *argv[])
{
	plan_tests(440);

	/* Large test. */
	test(MIN_USEFUL_SIZE * 2);

	/* Small test. */
	test(MIN_USEFUL_SIZE / 2);

	return exit_status();
}
