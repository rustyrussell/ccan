#include "alloc/alloc.h"
#include "tap/tap.h"
#include "alloc/alloc.c"
#include <stdlib.h>
#include <stdbool.h>

#define POOL_ORD 16
#define POOL_SIZE (1 << POOL_ORD)

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
		if (alloc_size(mem, poolsize, p[i]) < i)
			return false;
	return true;
}

int main(int argc, char *argv[])
{
	void *mem;
	unsigned int i, num;
	void *p[POOL_SIZE];

	plan_tests(5);

	/* FIXME: Needs to be page aligned for now. */
	posix_memalign(&mem, 1 << POOL_ORD, POOL_SIZE);

	alloc_init(mem, POOL_SIZE);

	/* Check that alloc_size() gives reasonable answers. */
	for (i = 0; i < POOL_SIZE; i++) {
		p[i] = alloc_get(mem, POOL_SIZE, i, 1);
		if (!p[i])
			break;
		invert_bytes(p[i], alloc_size(mem, POOL_SIZE, p[i]));
	}
	ok1(i < POOL_SIZE);
	num = i;
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(sizes_ok(mem, POOL_SIZE, p, num));

	/* Free every second one. */
	for (i = 0; i < num; i+=2) {
		alloc_free(mem, POOL_SIZE, p[i]);
		/* Compact. */
		if (i + 1 < num) {
			p[i/2] = p[i + 1];
			invert_bytes(p[i/2], alloc_size(mem,POOL_SIZE,p[i/2]));
		}
	}
	num /= 2;
	ok1(alloc_check(mem, POOL_SIZE));
	ok1(sizes_ok(mem, POOL_SIZE, p, num));

	return exit_status();
}
