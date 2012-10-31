/* This used to crash us on 64-bit; submitted by
   Alex Wulms <alex.wulms@scarlet.be> */
#include <ccan/crcsync/crcsync.h>
#include <ccan/crcsync/crcsync.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>

/* FIXME: ccanize. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef struct {
	int block_count;
	uint64_t *crcs;
} crc_info_t;

static void crcblocks(crc_info_t *crc_info, const char *data, int datalen, int blocksize)
{
	crc_info->block_count = (datalen+blocksize-1)/blocksize;
	crc_info->crcs = malloc(sizeof(uint64_t)*(crc_info->block_count + 1));
	crc_of_blocks(data, datalen, blocksize, 60, crc_info->crcs);
}

#define BLOCKSIZE 5

int main(int argc, char *argv[])
{
	/* Divided into BLOCKSIZE blocks */
	const char *data1 =
		"abcde" "fghij" "klmno" "pqrst" "uvwxy" "z ABC"
		"DEFGH" "IJKLM" "NOPQR" "STUVW" "XYZ 0" "12345" "6789";
	/* Divided into blocks that match. */
	const char *data2 =
		/* NO MATCH */
		"acde"
		/* MATCH */
		"fghij" "klmno" 
		/* NO MATCH */
		"pqr-a-very-long-test-that-differs-between-two-invokations-of-the-same-page-st"
		/* MATCH */
		"uvwxy" "z ABC" "DEFGH" "IJKLM" "NOPQR" "STUVW" "XYZ 0" "12345"
		"6789"
		/* NO MATCH */
		"ab";

	int expected[] = { 4,
			   -2, -3,
			   77,
			   -5, -6, -7, -8, -9, -10, -11, -12,
			   -13,
			   2 };
	crc_info_t crc_info1;
	struct crc_context *crcctx;
	long result;
	size_t ndigested;
	size_t offset = 0;
	size_t len2 = strlen(data2);
	size_t tailsize = strlen(data1) % BLOCKSIZE;
	int expected_i = 0;

	plan_tests(ARRAY_SIZE(expected) + 2);
	crcblocks(&crc_info1, data1, strlen(data1), BLOCKSIZE);

	crcctx = crc_context_new(BLOCKSIZE, 60, crc_info1.crcs, crc_info1.block_count,
				 tailsize);
	while ( offset < len2)
	{
		ndigested = crc_read_block(crcctx, &result, data2+offset, len2 - offset);
		offset += ndigested;
		if (result < 0)
			/* Match. */
			ok1(result == expected[expected_i++]);
		else {
			/* Literal. */
			ok1(result <= expected[expected_i]);
			expected[expected_i] -= result;
			if (!expected[expected_i])
				expected_i++;
		}
	}

	while ((result = crc_read_flush(crcctx)) != 0) {
		if (result < 0)
			/* Match. */
			ok1(result == expected[expected_i++]);
		else {
			/* Literal. */
			ok1(result <= expected[expected_i]);
			expected[expected_i] -= result;
			if (!expected[expected_i])
				expected_i++;
		}
	}
	ok1(expected_i == ARRAY_SIZE(expected));
	crc_context_free(crcctx);
	free(crc_info1.crcs);
	return 0;
}
