#include <ccan/invbloom/invbloom.h>
/* Include the C files directly. */
#include <ccan/invbloom/invbloom.c>
#include <ccan/tap/tap.h>

int main(void)
{
	struct invbloom *ib;
	const tal_t *ctx = tal(NULL, char);
	int val, val2, *ip, *ip2, i;

	/* This is how many tests you plan to run */
	plan_tests(127);

	ib = invbloom_new(ctx, int, 1, 100);
	ok1(tal_parent(ib) == ctx);
	ok1(ib->id_size == sizeof(int));
	ok1(ib->salt == 100);
	ok1(ib->n_elems == 1);
	ok1(invbloom_empty(ib));

	val = 0;
	invbloom_insert(ib, &val);
	ok1(ib->count[0] == NUM_HASHES);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val);
	ok1(invbloom_empty(ib));

	val2 = 2;
	invbloom_insert(ib, &val);
	invbloom_insert(ib, &val2);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val2);
	ok1(invbloom_empty(ib));

	tal_free(ib);

	ib = invbloom_new(ctx, int, 1, 100);
	ok1(tal_parent(ib) == ctx);
	ok1(ib->id_size == sizeof(int));
	ok1(ib->salt == 100);
	ok1(ib->n_elems == 1);
	ok1(invbloom_empty(ib));

	val = 0;
	invbloom_insert(ib, &val);
	ok1(ib->count[0] == NUM_HASHES);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val);
	ok1(invbloom_empty(ib));

	val2 = 2;
	invbloom_insert(ib, &val);
	invbloom_insert(ib, &val2);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val);
	ok1(!invbloom_empty(ib));
	invbloom_delete(ib, &val2);
	ok1(invbloom_empty(ib));

	tal_free(ib);

	/* Now, a more realistic test. */
	for (i = 0; i < 5; i++) {
		ib = invbloom_new(ctx, int, 1024, i);
		invbloom_insert(ib, &val);
		invbloom_insert(ib, &val2);
		ok1(invbloom_get(ib, &val));
		ok1(invbloom_get(ib, &val2));

		ip = invbloom_extract_negative(ctx, ib);
		ok1(!ip);

		ip = invbloom_extract(ctx, ib);
		ok1(ip);
		ok1(tal_parent(ip) == ctx);
		ok1(*ip == val || *ip == val2);

		ip2 = invbloom_extract(ctx, ib);
		ok1(ip2);
		ok1(tal_parent(ip2) == ctx);
		ok1(*ip2 == val || *ip2 == val2);
		ok1(*ip2 != *ip);

		ok1(invbloom_extract(ctx, ib) == NULL);

		invbloom_delete(ib, &val);
		invbloom_delete(ib, &val2);
		ip = invbloom_extract(ctx, ib);
		ok1(!ip);

		ip = invbloom_extract_negative(ctx, ib);
		ok1(ip);
		ok1(tal_parent(ip) == ctx);
		ok1(*ip == val || *ip == val2);

		ip2 = invbloom_extract_negative(ctx, ib);
		ok1(ip2);
		ok1(tal_parent(ip2) == ctx);
		ok1(*ip2 == val || *ip2 == val2);
		ok1(*ip2 != *ip);

		ok1(invbloom_extract_negative(ctx, ib) == NULL);
		ok1(invbloom_empty(ib));

		tal_free(ib);
	}

	tal_free(ctx);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
