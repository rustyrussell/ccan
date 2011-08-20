#include "common.h"

/*
 * Note that bdelta_diff verifies the patch before returning it (except for
 * when it returns a PT_LITERAL patch, as its correctness is easy to prove).
 * Only the trivial tests check the result explicitly using bdiff_patch.
 */
static int test_random(
	uint32_t old_size, uint32_t diff_size,
	unsigned int cardinality, unsigned int multiplier, unsigned int offset)
{
	struct rstring_byte_range range;
	uint8_t *old;
	uint8_t *new_;
	uint32_t new_size;
	BDELTAcode rc;
	
	range.cardinality = cardinality;
	range.multiplier = multiplier;
	range.offset = offset;
	
	if (random_string_pair(old_size, diff_size, cardinality == 0 ? NULL : &range,
	                       &old, &new_, &new_size) != RSTRING_OK)
	{
		fprintf(stderr, "Error generating random string pair\n");
		exit(EXIT_FAILURE);
	}
	
	rc = bdelta_diff(old, old_size, new_, new_size, NULL, NULL);
	if (rc != BDELTA_OK) {
		bdelta_perror("bdelta_diff", rc);
		return 0;
	}
	
	free(new_);
	free(old);
	return 1;
}

int main(void)
{
	int i;
	int count = 25;
	
	plan_tests(count * 14);
	
	for (i = 0; i < count; i++)
		ok1(test_random(100, 10, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(100, rand32() % 200, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(1000, rand32() % 200, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(1000, rand32() % 2000, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(10000, rand32() % 200, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(10000, rand32() % 2000, 0, 0, 0));
	for (i = 0; i < count; i++)
		ok1(test_random(rand32() % 20000, rand32() % 20000, 0, 0, 0));
	
	/* Low-cardinality tests */
	for (i = 0; i < count; i++)
		ok1(test_random(100, 10, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(100, rand32() % 200, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(1000, rand32() % 200, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(1000, rand32() % 2000, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(10000, rand32() % 200, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(10000, rand32() % 2000, rand32() % 20 + 1, 1, i));
	for (i = 0; i < count; i++)
		ok1(test_random(rand32() % 20000, rand32() % 20000, rand32() % 20 + 1, 1, i));
	
	return exit_status();
}
