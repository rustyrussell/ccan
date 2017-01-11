#include <ccan/intmap/intmap.h>
#include <ccan/intmap/intmap.c>
#include <ccan/tap/tap.h>

int main(void)
{
	SINTMAP(const char *) map;
	const char *first = "first", *second = "second";

	/* This is how many tests you plan to run */
	plan_tests(35);

	sintmap_init(&map);
	/* Test boundaries. */
	ok1(!sintmap_get(&map, 0x7FFFFFFFFFFFFFFFLL));
	ok1(!sintmap_get(&map, -0x8000000000000000LL));
	ok1(sintmap_first(&map) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_after(&map, 0x7FFFFFFFFFFFFFFFLL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_after(&map, -0x8000000000000000LL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_after(&map, 0x7FFFFFFFFFFFFFFELL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_add(&map, 0x7FFFFFFFFFFFFFFFLL, first));
	ok1(sintmap_get(&map, 0x7FFFFFFFFFFFFFFFLL) == first);
	ok1(sintmap_first(&map) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == 0);
	ok1(sintmap_add(&map, -0x8000000000000000LL, second));
	ok1(sintmap_get(&map, 0x7FFFFFFFFFFFFFFFLL) == first);
	ok1(sintmap_get(&map, -0x8000000000000000LL) == second);
	ok1(sintmap_first(&map) == -0x8000000000000000LL);
	ok1(sintmap_after(&map, -0x8000000000000000LL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == 0);
	ok1(sintmap_after(&map, 0x7FFFFFFFFFFFFFFELL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == 0);
	ok1(sintmap_after(&map, -0x7FFFFFFFFFFFFFFFLL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == 0);
	ok1(sintmap_after(&map, 0x7FFFFFFFFFFFFFFFLL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_del(&map, 0x7FFFFFFFFFFFFFFFLL) == first);
	ok1(sintmap_after(&map, -0x8000000000000000LL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == ENOENT);
	ok1(sintmap_add(&map, 0x7FFFFFFFFFFFFFFFLL, first));
	ok1(sintmap_del(&map, 0x8000000000000000LL) == second);
	ok1(sintmap_after(&map, -0x8000000000000000LL) == 0x7FFFFFFFFFFFFFFFLL);
	ok1(errno == 0);
	ok1(sintmap_del(&map, 0x7FFFFFFFFFFFFFFFLL) == first);
	ok1(sintmap_empty(&map));
	
	/* This exits depending on whether all tests passed */
	return exit_status();
}
