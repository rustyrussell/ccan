#include "crcsync/crcsync.h"
#include "tap/tap.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/* FIXME: ccanize. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct result {
	enum {
		LITERAL, BLOCK
	} type;
	/* Block number, or length of literal. */
	size_t val;
};

static inline size_t num_blocks(size_t len, size_t block_size)
{
	return (len + block_size - 1) / block_size;
}

static void check_finalized_result(size_t curr_literal,
				   const struct result results[],
				   size_t num_results,
				   size_t *curr_result)
{
	if (curr_literal == 0)
		return;
	ok1(*curr_result < num_results);
	ok1(results[*curr_result].type == LITERAL);
	ok1(results[*curr_result].val == curr_literal);
	(*curr_result)++;
}

static void check_result(long result,
			 size_t *curr_literal,
			 const struct result results[], size_t num_results,
			 size_t *curr_result)
{
	/* We append multiple literals into one. */
	if (result >= 0) {
		*curr_literal += result;
		return;
	}

	/* Check outstanding literals. */
	if (*curr_literal) {
		check_finalized_result(*curr_literal, results, num_results,
				       curr_result);
		*curr_literal = 0;
	}

	ok1(*curr_result < num_results);
	ok1(results[*curr_result].type == BLOCK);
	ok1(results[*curr_result].val == -result - 1);
	(*curr_result)++;
}

/* Start with buffer1 and sync to buffer2. */
static void test_sync(const char *buffer1, size_t len1,
		      const char *buffer2, size_t len2,
		      size_t block_size,
		      const struct result results[], size_t num_results)
{
	struct crc_context *ctx;
	size_t used, ret, i, curr_literal;
	long result;
	uint32_t crcs[num_blocks(len1, block_size)];

	crc_of_blocks(buffer1, len1, block_size, 32, crcs);

	/* Normal method. */
	ctx = crc_context_new(block_size, 32, crcs, ARRAY_SIZE(crcs));

	curr_literal = 0;
	for (used = 0, i = 0; used < len2; used += ret) {
		ret = crc_read_block(ctx, &result, buffer2+used, len2-used);
		check_result(result, &curr_literal, results, num_results, &i);
	}

	while ((result = crc_read_flush(ctx)) != 0)
		check_result(result, &curr_literal, results, num_results, &i);

	check_finalized_result(curr_literal, results, num_results, &i);
	
	/* We must have achieved everything we expected. */
	ok1(i == num_results);
	crc_context_free(ctx);

	/* Byte-at-a-time method. */
	ctx = crc_context_new(block_size, 32, crcs, ARRAY_SIZE(crcs));

	curr_literal = 0;
	for (used = 0, i = 0; used < len2; used += ret) {
		ret = crc_read_block(ctx, &result, buffer2+used, 1);

		check_result(result, &curr_literal, results, num_results, &i);
	}

	while ((result = crc_read_flush(ctx)) != 0)
		check_result(result, &curr_literal, results, num_results, &i);

	check_finalized_result(curr_literal, results, num_results, &i);
	
	/* We must have achieved everything we expected. */
	ok1(i == num_results);
	crc_context_free(ctx);
}

int main(int argc, char *argv[])
{
	char *buffer1, *buffer2;
	unsigned int i;
	uint32_t crcs1[12], crcs2[12];

	plan_tests(1454);

	buffer1 = calloc(1024, 1);
	buffer2 = calloc(1024, 1);

	/* Truncated end block test. */
	crcs1[11] = 0xdeadbeef;
	crc_of_blocks(buffer1, 1024, 100, 32, crcs1);
	ok1(crcs1[11] == 0xdeadbeef);
	crc_of_blocks(buffer2, 1024, 100, 32, crcs2);
	ok1(memcmp(crcs1, crcs2, sizeof(crcs1[0])*11) == 0);

	/* Fill with non-zero pattern, retest. */
	for (i = 0; i < 1024; i++)
		buffer1[i] = buffer2[i] = i + i/128;

	crcs1[11] = 0xdeadbeef;
	crc_of_blocks(buffer1, 1024, 100, 32, crcs1);
	ok1(crcs1[11] == 0xdeadbeef);
	crc_of_blocks(buffer2, 1024, 100, 32, crcs2);
	ok1(memcmp(crcs1, crcs2, sizeof(crcs1[0])*11) == 0);

	/* Check that it correctly masks bits. */
	crc_of_blocks(buffer1, 1024, 128, 32, crcs1);
	crc_of_blocks(buffer2, 1024, 128, 8, crcs2);
	for (i = 0; i < 1024/128; i++)
		ok1(crcs2[i] == (crcs1[i] & 0xFF));

	/* Now test the "exact match" "round blocks" case. */
	{
		struct result res[] = {
			{ BLOCK, 0 },
			{ BLOCK, 1 },
			{ BLOCK, 2 },
			{ BLOCK, 3 },
			{ BLOCK, 4 },
			{ BLOCK, 5 },
			{ BLOCK, 6 },
			{ BLOCK, 7 } };
		test_sync(buffer1, 1024, buffer2, 1024, 128,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "exact match" with end block case. */
	{
		struct result res[] = {
			{ BLOCK, 0 },
			{ BLOCK, 1 },
			{ BLOCK, 2 },
			{ BLOCK, 3 },
			{ BLOCK, 4 },
			{ BLOCK, 5 },
			{ BLOCK, 6 },
			{ BLOCK, 7 },
			{ BLOCK, 8 },
			{ BLOCK, 9 },
			{ BLOCK, 10 } };
		test_sync(buffer1, 1024, buffer2, 1024, 100,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "one byte append" "round blocks" case. */
	{
		struct result res[] = {
			{ BLOCK, 0 },
			{ BLOCK, 1 },
			{ BLOCK, 2 },
			{ BLOCK, 3 },
			{ BLOCK, 4 },
			{ BLOCK, 5 },
			{ BLOCK, 6 },
			{ LITERAL, 1 } };
		test_sync(buffer1, 1024-128, buffer2, 1024-127, 128,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "one byte append" with end block case. */
	{
		struct result res[] = {
			{ BLOCK, 0 },
			{ BLOCK, 1 },
			{ BLOCK, 2 },
			{ BLOCK, 3 },
			{ BLOCK, 4 },
			{ BLOCK, 5 },
			{ BLOCK, 6 },
			{ BLOCK, 7 },
			{ BLOCK, 8 },
			{ BLOCK, 9 },
			{ BLOCK, 10 },
			{ LITERAL, 1 } };
		test_sync(buffer1, 1023, buffer2, 1024, 100,
			  res, ARRAY_SIZE(res));
	}

	/* Now try changing one block at a time, check we get right results. */
	for (i = 0; i < 1024/128; i++) {
		unsigned int j;
		struct result res[8];

		/* Mess with block. */
		memcpy(buffer2, buffer1, 1024);
		buffer2[i * 128]++;

		for (j = 0; j < ARRAY_SIZE(res); j++) {
			if (j == i) {
				res[j].type = LITERAL;
				res[j].val = 128;
			} else {
				res[j].type = BLOCK;
				res[j].val = j;
			}
		}

		test_sync(buffer1, 1024, buffer2, 1024, 128,
			  res, ARRAY_SIZE(res));
	}

	/* Now try shrinking one block at a time, check we get right results. */
	for (i = 0; i < 1024/128; i++) {
		unsigned int j;
		struct result res[8];

		/* Shrink block. */
		memcpy(buffer2, buffer1, i * 128 + 64);
		memcpy(buffer2 + i * 128 + 64, buffer1 + i * 128 + 65,
		       1024 - (i * 128 + 65));

		for (j = 0; j < ARRAY_SIZE(res); j++) {
			if (j == i) {
				res[j].type = LITERAL;
				res[j].val = 127;
			} else {
				res[j].type = BLOCK;
				res[j].val = j;
			}
		}

		test_sync(buffer1, 1024, buffer2, 1023, 128,
			  res, ARRAY_SIZE(res));
	}

	/* Now try shrinking one block at a time, check we get right results. */
	for (i = 0; i < 1024/128; i++) {
		unsigned int j;
		struct result res[8];

		/* Shrink block. */
		memcpy(buffer2, buffer1, i * 128 + 64);
		memcpy(buffer2 + i * 128 + 64, buffer1 + i * 128 + 65,
		       1024 - (i * 128 + 65));

		for (j = 0; j < ARRAY_SIZE(res); j++) {
			if (j == i) {
				res[j].type = LITERAL;
				res[j].val = 127;
			} else {
				res[j].type = BLOCK;
				res[j].val = j;
			}
		}

		test_sync(buffer1, 1024, buffer2, 1023, 128,
			  res, ARRAY_SIZE(res));
	}

	return exit_status();
}
