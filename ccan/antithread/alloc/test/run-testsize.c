#include <ccan/antithread/alloc/alloc.h>
#include <ccan/tap/tap.h>
#include <ccan/antithread/alloc/alloc.c>
#include <ccan/antithread/alloc/bitops.c>
#include <ccan/antithread/alloc/tiny.c>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>

static void invert_bytes(unsigned char *p, unsigned long size)
{
	unsigned int i;

	for (i = 0; i < size; i++)
		p[i] ^= 0xFF;
}

static bool sizes_ok(void *mem, unsigned long poolsize, void *p[], unsigned num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		if (p[i] && alloc_size(mem, poolsize, p[i]) < i)
			return false;
	return true;
}

static void test_pool(unsigned long pool_size)
{
	unsigned int i, num;
	void *mem;
	void **p;
	bool flip = false;

	p = calloc(pool_size, sizeof(void *));
	mem = malloc(pool_size);

	alloc_init(mem, pool_size);

	/* Check that alloc_size() gives reasonable answers. */
	for (i = 0; i < pool_size; i = i * 3 / 2 + 1) {
		p[i] = alloc_get(mem, pool_size, i, 1);
		if (!p[i])
			break;
		invert_bytes(p[i], alloc_size(mem, pool_size, p[i]));
	}
	ok1(i < pool_size);
	num = i;
	ok1(alloc_check(mem, pool_size));
	ok1(sizes_ok(mem, pool_size, p, num));

	/* Free every second one. */
	for (i = 0; i < num; i = i * 3 / 2 + 1) {
		flip = !flip;
		if (flip) {
			invert_bytes(p[i], alloc_size(mem,pool_size,p[i]));
			continue;
		}
		alloc_free(mem, pool_size, p[i]);
		p[i] = NULL;
	}
	ok1(alloc_check(mem, pool_size));
	ok1(sizes_ok(mem, pool_size, p, num));
	free(p);
	free(mem);
}

int main(int argc, char *argv[])
{
	plan_tests(10);

	/* Large test. */
	test_pool(MIN_USEFUL_SIZE * 2);

	/* Small test. */
	test_pool(MIN_USEFUL_SIZE / 2);

	return exit_status();
}
