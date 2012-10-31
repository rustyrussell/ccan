#include <ccan/crcsync/crcsync.h>
#include <ccan/crcsync/crcsync.c>
#include <ccan/tap/tap.h>
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
	size_t used, ret, i, curr_literal, tailsize;
	long result;
	uint64_t crcs[num_blocks(len1, block_size)];

	crc_of_blocks(buffer1, len1, block_size, 64, crcs);

	tailsize = len1 % block_size;

	/* Normal method. */
	ctx = crc_context_new(block_size, 64, crcs, ARRAY_SIZE(crcs),
			      tailsize);

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
	ctx = crc_context_new(block_size, 64, crcs, ARRAY_SIZE(crcs),
			      tailsize);

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

#define BUFFER_SIZE 512
#define BLOCK_SIZE 128
#define NUM_BLOCKS (BUFFER_SIZE / BLOCK_SIZE)

int main(int argc, char *argv[])
{
	char *buffer1, *buffer2;
	unsigned int i;
	uint64_t crcs1[NUM_BLOCKS], crcs2[NUM_BLOCKS];

	plan_tests(664);

	buffer1 = calloc(BUFFER_SIZE, 1);
	buffer2 = calloc(BUFFER_SIZE, 1);

	/* Truncated end block test. */
	crcs1[ARRAY_SIZE(crcs1)-1] = 0xdeadbeef;
	crc_of_blocks(buffer1, BUFFER_SIZE-BLOCK_SIZE-1, BLOCK_SIZE, 64, crcs1);
	ok1(crcs1[ARRAY_SIZE(crcs1)-1] == 0xdeadbeef);
	crc_of_blocks(buffer2, BUFFER_SIZE-BLOCK_SIZE-1, BLOCK_SIZE, 64, crcs2);
	ok1(memcmp(crcs1, crcs2, sizeof(crcs1[0])*(ARRAY_SIZE(crcs1)-1)) == 0);

	/* Fill with non-zero pattern, retest. */
	for (i = 0; i < BUFFER_SIZE; i++)
		buffer1[i] = buffer2[i] = i + i/BLOCK_SIZE;

	crcs1[ARRAY_SIZE(crcs1)-1] = 0xdeadbeef;
	crc_of_blocks(buffer1, BUFFER_SIZE-BLOCK_SIZE-1, BLOCK_SIZE, 64, crcs1);
	ok1(crcs1[ARRAY_SIZE(crcs1)-1] == 0xdeadbeef);
	crc_of_blocks(buffer2, BUFFER_SIZE-BLOCK_SIZE-1, BLOCK_SIZE, 64, crcs2);
	ok1(memcmp(crcs1, crcs2, sizeof(crcs1[0])*(ARRAY_SIZE(crcs1)-1)) == 0);

	/* Check that it correctly masks bits. */
	crc_of_blocks(buffer1, BUFFER_SIZE, BLOCK_SIZE, 64, crcs1);
	crc_of_blocks(buffer2, BUFFER_SIZE, BLOCK_SIZE, 8, crcs2);
	for (i = 0; i < NUM_BLOCKS; i++)
		ok1(crcs2[i] == (crcs1[i] & 0xFF00000000000000ULL));

	/* Now test the "exact match" "round blocks" case. */
	{
		struct result res[NUM_BLOCKS];

		for (i = 0; i < ARRAY_SIZE(res); i++) {
			res[i].type = BLOCK;
			res[i].val = i;
		}
		test_sync(buffer1, BUFFER_SIZE, buffer2,
			  BUFFER_SIZE, BLOCK_SIZE,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "exact match" with end block case. */
	{
		struct result res[NUM_BLOCKS+1];
		for (i = 0; i < ARRAY_SIZE(res); i++) {
			res[i].type = BLOCK;
			res[i].val = i;
		}
		test_sync(buffer1, BUFFER_SIZE, buffer2,
			  BUFFER_SIZE, BLOCK_SIZE-1,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "one byte append" "round blocks" case. */
	{
		struct result res[NUM_BLOCKS];
		for (i = 0; i < ARRAY_SIZE(res)-1; i++) {
			res[i].type = BLOCK;
			res[i].val = i;
		}
		res[i].type = LITERAL;
		res[i].val = 1;

		test_sync(buffer1, BUFFER_SIZE-BLOCK_SIZE,
			  buffer2, BUFFER_SIZE-BLOCK_SIZE+1, BLOCK_SIZE,
			  res, ARRAY_SIZE(res));
	}

	/* Now test the "one byte append" with end block case. */
	{
		struct result res[NUM_BLOCKS+2];
		for (i = 0; i < ARRAY_SIZE(res)-1; i++) {
			res[i].type = BLOCK;
			res[i].val = i;
		}
		res[i].type = LITERAL;
		res[i].val = 1;

		test_sync(buffer1, BUFFER_SIZE-1,
			  buffer2, BUFFER_SIZE,
			  BLOCK_SIZE - 1, res, ARRAY_SIZE(res));
	}

	/* Now try changing one block at a time, check we get right results. */
	for (i = 0; i < NUM_BLOCKS; i++) {
		unsigned int j;
		struct result res[NUM_BLOCKS];

		/* Mess with block. */
		memcpy(buffer2, buffer1, BUFFER_SIZE);
		buffer2[i * BLOCK_SIZE]++;

		for (j = 0; j < ARRAY_SIZE(res); j++) {
			if (j == i) {
				res[j].type = LITERAL;
				res[j].val = BLOCK_SIZE;
			} else {
				res[j].type = BLOCK;
				res[j].val = j;
			}
		}

		test_sync(buffer1, BUFFER_SIZE,
			  buffer2, BUFFER_SIZE,
			  BLOCK_SIZE, res, ARRAY_SIZE(res));
	}

	/* Now try shrinking one block at a time, check we get right results. */
	for (i = 0; i < NUM_BLOCKS; i++) {
		unsigned int j;
		struct result res[NUM_BLOCKS];

		/* Shrink block. */
		memcpy(buffer2, buffer1, i * BLOCK_SIZE + BLOCK_SIZE/2);
		memcpy(buffer2 + i * BLOCK_SIZE + BLOCK_SIZE/2,
		       buffer1 + i * BLOCK_SIZE + BLOCK_SIZE/2 + 1,
		       BUFFER_SIZE - (i * BLOCK_SIZE + BLOCK_SIZE/2 + 1));

		for (j = 0; j < ARRAY_SIZE(res); j++) {
			if (j == i) {
				res[j].type = LITERAL;
				res[j].val = BLOCK_SIZE-1;
			} else {
				res[j].type = BLOCK;
				res[j].val = j;
			}
		}

		test_sync(buffer1, BUFFER_SIZE,
			  buffer2, BUFFER_SIZE-1,
			  BLOCK_SIZE, res, ARRAY_SIZE(res));
	}

	/* Finally, all possible combinations. */
	for (i = 0; i < (1 << NUM_BLOCKS); i++) {
		unsigned int j, num_res;
		struct result res[NUM_BLOCKS];

		memcpy(buffer2, buffer1, BUFFER_SIZE);
		for (j = num_res = 0; j < ARRAY_SIZE(res); j++) {
			if (i & (i << j)) {
				res[num_res].type = BLOCK;
				res[num_res].val = j;
				num_res++;
			} else {
				/* Mess with block. */
				buffer2[j * BLOCK_SIZE]++;
				if (num_res && res[num_res-1].type == LITERAL)
					res[num_res-1].val += BLOCK_SIZE;
				else {
					res[num_res].type = LITERAL;
					res[num_res].val = BLOCK_SIZE;
					num_res++;
				}
			}
		}

		test_sync(buffer1, BUFFER_SIZE,
			  buffer2, BUFFER_SIZE,
			  BLOCK_SIZE, res, num_res);
	}

	free(buffer1);
	free(buffer2);
	return exit_status();
}
