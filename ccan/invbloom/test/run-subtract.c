#include <ccan/invbloom/invbloom.h>
/* Include the C files directly. */
#include <ccan/invbloom/invbloom.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct invbloom *ib1, *ib2;
	const tal_t *ctx = tal(NULL, char);
	int val = 1, val2 = 2, *ip;

	/* This is how many tests you plan to run */
	plan_tests(8);

	ib1 = invbloom_new(ctx, int, 1024, 0);
	ib2 = invbloom_new(ctx, int, 1024, 0);
	invbloom_insert(ib1, &val);
	invbloom_insert(ib2, &val2);

	invbloom_subtract(ib1, ib2);

	ip = invbloom_extract(ctx, ib1);
	ok1(ip);
	ok1(tal_parent(ip) == ctx);
	ok1(*ip == val);

	ip = invbloom_extract(ctx, ib1);
	ok1(!ip);

	ip = invbloom_extract_negative(ctx, ib1);
	ok1(ip);
	ok1(tal_parent(ip) == ctx);
	ok1(*ip == val2);

	ip = invbloom_extract_negative(ctx, ib1);
	ok1(!ip);

	tal_free(ctx);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
