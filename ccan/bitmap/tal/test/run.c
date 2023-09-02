#include <ccan/bitmap/tal/tal.h>
#include <ccan/tap/tap.h>

static size_t bitmap_popcount(const bitmap *b, size_t nbits)
{
	size_t i, n = 0;
	for (i = 0; i < nbits; i++)
		n += bitmap_test_bit(b, i);
	return n;
}

int main(void)
{
	tal_t *ctx = tal(NULL, char);
	bitmap *b;

	/* This is how many tests you plan to run */
	plan_tests(10);
	b = bitmap_talz(ctx, 99);
	ok1(bitmap_empty(b, 99));

	b = bitmap_tal_fill(ctx, 99);
	ok1(bitmap_full(b, 99));

	/* Resize shrink (with zero pad). */
	ok1(bitmap_tal_resizez(&b, 99, 50));
	ok1(bitmap_full(b, 50));

	/* Resize shrink (with one pad). */
	ok1(bitmap_tal_resize_fill(&b, 50, 29));
	ok1(bitmap_full(b, 29));

	/* Enlarge (with zero pad) */
	ok1(bitmap_tal_resizez(&b, 29, 50));
	ok1(bitmap_popcount(b, 50) == 29);

	/* Enlarge (with one pad) */
	ok1(bitmap_tal_resize_fill(&b, 50, 99));
	ok1(bitmap_popcount(b, 99) == 29 + 49);

	tal_free(ctx);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
