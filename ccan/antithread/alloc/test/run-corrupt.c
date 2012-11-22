/* Example allocation which caused corruption. */
#include <ccan/antithread/alloc/alloc.c>
#include <ccan/antithread/alloc/bitops.c>
#include <ccan/antithread/alloc/tiny.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	void *mem;

	plan_tests(7);

	mem = malloc(1179648);
	alloc_init(mem, 1179648);
	ok1(alloc_check(mem, 1179648));
	ok1(alloc_get(mem, 1179648, 48, 16));
	ok1(alloc_check(mem, 1179648));
	ok1(alloc_get(mem, 1179648, 53, 16));
	ok1(alloc_check(mem, 1179648));
	ok1(alloc_get(mem, 1179648, 53, 16));
	ok1(alloc_check(mem, 1179648));
	free(mem);

	return exit_status();
}
