#include <ccan/tap/tap.h>
#include "config.h"
#include <ccan/alloc/tiny.c>
#include <ccan/alloc/bitops.c>
#include <stdlib.h>
#include <err.h>

/* Test encoding and decoding. */
#define ARR_SIZE 10

int main(void)
{
	unsigned char array[ARR_SIZE];
	unsigned int i, prev;

	plan_tests(567);

	prev = 0;
	/* Test encode_length */
	for (i = 1; i < 0x8000000; i *= 2) {
		ok1(encode_length(i-1) >= prev);
		ok1(encode_length(i) >= encode_length(i-1));
		ok1(encode_length(i+1) >= encode_length(i));
		prev = encode_length(i);
	}

	/* Test it against actual encoding return val. */
	for (i = 1; i < 0x8000000; i *= 2) {
		ok1(encode_length(i-1) == encode(i - 1 + MIN_BLOCK_SIZE,
						 false, array, ARR_SIZE));
		ok1(encode_length(i) == encode(i + MIN_BLOCK_SIZE,
					       false, array, ARR_SIZE));
		ok1(encode_length(i+1) == encode(i + 1 + MIN_BLOCK_SIZE,
						 false, array, ARR_SIZE));
	}

	/* Test encoder vs. decoder. */
	for (i = 1; i < 0x8000000; i *= 2) {
		unsigned long hdrlen, len;
		bool free;

		hdrlen = encode(i - 1 + MIN_BLOCK_SIZE, false, array, ARR_SIZE);
		ok1(decode(&len, &free, array) == hdrlen);
		ok1(len == i - 1 + MIN_BLOCK_SIZE);
		ok1(free == false);

		hdrlen = encode(i + MIN_BLOCK_SIZE, true, array, ARR_SIZE);
		ok1(decode(&len, &free, array) == hdrlen);
		ok1(len == i + MIN_BLOCK_SIZE);
		ok1(free == true);

		hdrlen = encode(i + 1 + MIN_BLOCK_SIZE, true, array, ARR_SIZE);
		ok1(decode(&len, &free, array) == hdrlen);
		ok1(len == i + 1 + MIN_BLOCK_SIZE);
		ok1(free == true);
	}

	/* Test encoder limit enforcement. */
	for (i = 1; i < 0x8000000; i *= 2) {
		unsigned char *arr;
		unsigned int len;

		/* These should fit. */
		ok1(encode(i-1 + MIN_BLOCK_SIZE, false, array,
			   encode_length(i-1)) == encode_length(i-1));
		ok1(encode(i + MIN_BLOCK_SIZE, false, array,
			   encode_length(i)) == encode_length(i));
		ok1(encode(i+1 + MIN_BLOCK_SIZE, false, array,
			   encode_length(i+1)) == encode_length(i+1));

		/* These should not: malloc so valgrind finds overruns. */
		len = encode_length(i-1) - 1;
		arr = malloc(len);
		ok1(encode(i-1 + MIN_BLOCK_SIZE, true, arr, len) == 0);
		free(arr);

		len = encode_length(i-1) - 1;
		arr = malloc(len);
		ok1(encode(i + MIN_BLOCK_SIZE, false, arr, len) == 0);
		free(arr);

		len = encode_length(i+1) - 1;
		arr = malloc(len);
		ok1(encode(i+1 + MIN_BLOCK_SIZE, false, arr, len) == 0);
		free(arr);
	}
	return exit_status();
}
