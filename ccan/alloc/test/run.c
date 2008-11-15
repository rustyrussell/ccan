#include "alloc/alloc.h"
#include "tap/tap.h"
#include "alloc/alloc.c"
#include <stdlib.h>
#include <err.h>

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

static bool free_every_second_one(void *mem, unsigned int num, void *p[])
{
	unsigned int i;

	/* Free every second one. */
	for (i = 0; i < num; i += 2) {
		alloc_free(mem, POOL_SIZE, p[i]);
		if (!alloc_check(mem, POOL_SIZE))
			return false;
	}
	for (i = 1; i < num; i += 2) {
		alloc_free(mem, POOL_SIZE, p[i]);
		if (!alloc_check(mem, POOL_SIZE))
			return false;
	}
	return true;
}


int main(int argc, char *argv[])
{
	void *mem;
	unsigned int i, num, max_size;
	void *p[POOL_SIZE];

	plan_tests(178);

	/* FIXME: Needs to be page aligned for now. */
	if (posix_memalign(&mem, 1 << POOL_ORD, POOL_SIZE) != 0)
		errx(1, "Failed allocating aligned memory"); 

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
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= max_size);

	/* Free it, should be able to reallocate it. */
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= max_size);
	ok1(alloc_check(mem, POOL_SIZE));
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Allocate a whole heap. */
	for (i = 0; i < POOL_SIZE; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, 1, 1);
		if (!p[i])
			break;
	}

	/* Uncomment this for a more intuitive view of what the
	 * allocator looks like after all these 1 byte allocs. */
#if 0
	alloc_visualize(stderr, mem, POOL_SIZE);
#endif

	num = i;
	/* Can't allocate this many. */
	ok1(num != POOL_SIZE);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Sort them. */
	sort(p, num, addr_cmp);

	/* Uniqueness check */
	ok1(unique(p, num));

	ok1(free_every_second_one(mem, num, p));
	ok1(alloc_check(mem, POOL_SIZE));

	/* Should be able to reallocate max size. */
	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= max_size);

	/* Re-initializing should be the same as freeing everything */
	alloc_init(mem, POOL_SIZE);
	ok1(alloc_check(mem, POOL_SIZE));
	p[0] = alloc_get(mem, POOL_SIZE, max_size, 1);
	ok1(p[0]);
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= max_size);
	ok1(alloc_check(mem, POOL_SIZE));
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Alignment constraints should be met, as long as powers of two */
	for (i = 0; i < POOL_ORD-1; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, i, 1 << i);
		ok1(p[i]);
		ok1(((unsigned long)p[i] % (1 << i)) == 0);
		ok1(alloc_check(mem, POOL_SIZE));
		ok1(alloc_size(mem, POOL_SIZE, p[i]) >= i);
	}

	for (i = 0; i < POOL_ORD-1; i++) {
		alloc_free(mem, POOL_SIZE, p[i]);
		ok1(alloc_check(mem, POOL_SIZE));
	}

	/* Alignment constraints for a single-byte allocation. */
	for (i = 0; i < POOL_ORD; i++) {
		p[0] = alloc_get(mem, POOL_SIZE, 1, 1 << i);
		ok1(p[0]);
		ok1(alloc_check(mem, POOL_SIZE));
		ok1(alloc_size(mem, POOL_SIZE, p[i]) >= 1);
		alloc_free(mem, POOL_SIZE, p[0]);
		ok1(alloc_check(mem, POOL_SIZE));
	}

	/* Alignment check for a 0-byte allocation.  Corner case. */
	p[0] = alloc_get(mem, POOL_SIZE, 0, 1 << (POOL_ORD - 1));
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(alloc_size(mem, POOL_SIZE, p[0]) < POOL_SIZE);
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));

	/* Force the testing of split metadata. */
	alloc_init(mem, POOL_SIZE);
	for (i = 0; i < POOL_SIZE; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, getpagesize(), getpagesize());
		if (!p[i])
			break;
	}
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(alloc_size(mem, POOL_SIZE, p[i-1]) >= getpagesize());

	/* Sort them. */
	sort(p, i-1, addr_cmp);

	/* Free all but the one next to the metadata. */
	for (i = 1; p[i]; i++)
		alloc_free(mem, POOL_SIZE, p[i]);
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= getpagesize());

	/* Now do a whole heap of subpage allocs. */
	for (i = 1; i < POOL_SIZE; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, 1, 1);
		if (!p[i])
			break;
	}
	ok1(alloc_check(mem, POOL_SIZE));

	/* Free up our page next to metadata, and should be able to alloc */
	alloc_free(mem, POOL_SIZE, p[0]);
	ok1(alloc_check(mem, POOL_SIZE));
	p[0] = alloc_get(mem, POOL_SIZE, 1, 1);
	ok1(p[0]);
	ok1(alloc_size(mem, POOL_SIZE, p[0]) >= 1);

	/* Clean up. */
	for (i = 0; p[i]; i++)
		alloc_free(mem, POOL_SIZE, p[i]);
	ok1(alloc_check(mem, POOL_SIZE));

	return exit_status();
}
