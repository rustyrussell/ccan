#include <ccan/tal/tal.h>
#include <ccan/tal/tal.c>
#include <ccan/tap/tap.h>

static int error_count;

static void my_error(const char *msg)
{
	error_count++;
}

int main(void)
{
	void *p;
	int *pi, *origpi;

	plan_tests(26);

	tal_set_backend(NULL, NULL, NULL, my_error);

	p = tal_arr(NULL, int, (size_t)-1);
	ok1(!p);
	ok1(error_count == 1);

	p = tal_arr(NULL, char, (size_t)-2);
	ok1(!p);
	ok1(error_count == 2);

	/* Now try overflow cases for tal_dup. */
	error_count = 0;
	pi = origpi = tal_arr(NULL, int, 100);
	ok1(pi);
	ok1(error_count == 0);
	pi = tal_dup(NULL, int, pi, (size_t)-1, 0);
	ok1(!pi);
	ok1(error_count == 1);
	pi = tal_dup(NULL, int, pi, 0, (size_t)-1);
	ok1(!pi);
	ok1(error_count == 2);

	pi = tal_dup(NULL, int, pi, (size_t)-1UL / sizeof(int),
		     (size_t)-1UL / sizeof(int));
	ok1(!pi);
	ok1(error_count == 3);
	/* This will still overflow when tal_hdr is added. */
	pi = tal_dup(NULL, int, pi, (size_t)-1UL / sizeof(int) / 2,
		     (size_t)-1UL / sizeof(int) / 2);
	ok1(!pi);
	ok1(error_count == 4);

	/* Now, check that with TAL_TAKE we free old one on failure. */
	pi = tal_arr(NULL, int, 100);
	error_count = 0;
	pi = tal_dup(TAL_TAKE, int, pi, (size_t)-1, 0);
	ok1(!pi);
	ok1(error_count == 1);
	ok1(tal_first(NULL) == origpi && !tal_next(NULL, origpi));

	pi = tal_arr(NULL, int, 100);
	error_count = 0;
	pi = tal_dup(TAL_TAKE, int, pi, 0, (size_t)-1);
	ok1(!pi);
	ok1(error_count == 1);
	ok1(tal_first(NULL) == origpi && !tal_next(NULL, origpi));

	pi = tal_arr(NULL, int, 100);
	error_count = 0;
	pi = tal_dup(TAL_TAKE, int, pi, (size_t)-1UL / sizeof(int),
		     (size_t)-1UL / sizeof(int));
	ok1(!pi);
	ok1(error_count == 1);
	ok1(tal_first(NULL) == origpi && !tal_next(NULL, origpi));

	pi = tal_arr(NULL, int, 100);
	error_count = 0;
	/* This will still overflow when tal_hdr is added. */
	pi = tal_dup(TAL_TAKE, int, pi, (size_t)-1UL / sizeof(int) / 2,
		     (size_t)-1UL / sizeof(int) / 2);
	ok1(!pi);
	ok1(error_count == 1);
	ok1(tal_first(NULL) == origpi && !tal_next(NULL, origpi));

	return exit_status();
}
