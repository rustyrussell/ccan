#include <ccan/tally/tally.c>
#include <ccan/tap/tap.h>

int main(void)
{
	unsigned int i, max_step;
	ssize_t min, max;

	max = (ssize_t)~(1ULL << (sizeof(max)*CHAR_BIT - 1));
	min = (ssize_t)(1ULL << (sizeof(max)*CHAR_BIT - 1));
	max_step = sizeof(max)*CHAR_BIT;

	plan_tests(2 + 100 + 10 + 5
		   + 2 + 100 + 5 + 4
		   + (1 << 7) * (max_step - 7));

	/* Single step, single bucket == easy. */ 
	ok1(bucket_of(0, 0, 0) == 0);

	/* Double step, still in first bucket. */
	ok1(bucket_of(0, 1, 0) == 0);

	/* Step 8. */
	for (i = 0; i < 100; i++)
		ok1(bucket_of(0, 3, i) == i >> 3);

	/* 10 values in 5 buckets, step 2. */
	for (i = 0; i < 10; i++)
		ok1(bucket_of(0, 1, i) == i >> 1);

	/* Extreme cases. */
	ok1(bucket_of(min, 0, min) == 0);
	ok1(bucket_of(min, max_step-1, min) == 0);
	ok1(bucket_of(min, max_step-1, max) == 1);
	ok1(bucket_of(min, max_step, min) == 0);
	ok1(bucket_of(min, max_step, max) == 0);

	/* Now, bucket_min() should match: */
	ok1(bucket_min(0, 0, 0) == 0);

	/* Double step, val in first bucket still 0. */
	ok1(bucket_min(0, 1, 0) == 0);

	/* Step 8. */
	for (i = 0; i < 100; i++)
		ok1(bucket_min(0, 3, i) == i << 3);

	/* 10 values in 5 buckets, step 2. */
	for (i = 0; i < 5; i++)
		ok1(bucket_min(0, 1, i) == i << 1);

	/* Extreme cases. */
	ok1(bucket_min(min, 0, 0) == min);
	ok1(bucket_min(min, max_step-1, 0) == min);
	ok1(bucket_min(min, max_step-1, 1) == 0);
	ok1(bucket_min(min, max_step, 0) == min);

	/* Now, vary step and number of buckets, but bucket_min and bucket_of
	 * must agree. */
	for (i = 0; i < (1 << 7); i++) {
		unsigned int j;
		for (j = 0; j < max_step - 7; j++) {
			ssize_t val;

			val = bucket_min(-(ssize_t)i, j, i);
			ok1(bucket_of(-(ssize_t)i, j, val) == i);
		}
	}

	return exit_status();
}
