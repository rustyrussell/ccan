#include <ccan/invbloom/invbloom.h>
/* Include the C files directly. */
#include <ccan/invbloom/invbloom.c>
#include <ccan/tap/tap.h>

static void singleton_cb(struct invbloom *ib, size_t n,
			 unsigned *count)
{
	ok1(ib->count[n] == 1 || ib->count[n] == -1);
	(*count)++;
}	

int main(void)
{
	struct invbloom *ib;
	const tal_t *ctx = tal(NULL, char);
	int val;
	unsigned singleton_count;

	/* This is how many tests you plan to run */
	plan_tests(10 + 3 + NUM_HASHES * 2);

	/* Single entry ib table keeps it simple. */
	ib = invbloom_new(ctx, int, 1, 100);
	invbloom_singleton_cb(ib, singleton_cb, &singleton_count);

	val = 0;
	singleton_count = 0;
	invbloom_insert(ib, &val);
	ok1(ib->count[0] == NUM_HASHES);
	ok1(singleton_count == 1);
	ok1(!invbloom_empty(ib));

	/* First delete takes it via singleton. */
	invbloom_delete(ib, &val);
	ok1(singleton_count == 2);
	ok1(invbloom_empty(ib));

	/* Second delete creates negative singleton. */
	invbloom_delete(ib, &val);
	ok1(singleton_count == 3);

	/* Now a larger table: this seed set so entries don't clash */
	ib = invbloom_new(ctx, int, 1024, 0);
	singleton_count = 0;
	invbloom_singleton_cb(ib, singleton_cb, &singleton_count);

	val = 0;
	invbloom_insert(ib, &val);
	ok1(singleton_count == NUM_HASHES);

	/* First delete does nothing. */
	invbloom_delete(ib, &val);
	ok1(singleton_count == NUM_HASHES);
	ok1(invbloom_empty(ib));

	/* Second delete creates negative singletons. */
	invbloom_delete(ib, &val);
	ok1(singleton_count == NUM_HASHES * 2);

	tal_free(ctx);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
