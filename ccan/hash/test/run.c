#include "hash/hash.h"
#include "tap/tap.h"
#include "hash/hash.c"
#include <stdbool.h>
#include <string.h>

#define ARRAY_WORDS 5

int main(int argc, char *argv[])
{
	unsigned int i, j, k;
	uint32_t array[ARRAY_WORDS], val;
	char array2[sizeof(array) + sizeof(uint32_t)];
	uint32_t results[256];

	/* Initialize array. */
	for (i = 0; i < ARRAY_WORDS; i++)
		array[i] = i;

	plan_tests(55);

	/* hash_stable is guaranteed. */
	ok1(hash_stable(array, ARRAY_WORDS, 0) == 0x13305f8c);
	ok1(hash_stable(array, ARRAY_WORDS, 1) == 0x171abf74);
	ok1(hash_stable(array, ARRAY_WORDS, 2) == 0x7646fcc7);
	ok1(hash_stable(array, ARRAY_WORDS, 4) == 0xa758ed5);
	ok1(hash_stable(array, ARRAY_WORDS, 8) == 0x2dedc2e4);
	ok1(hash_stable(array, ARRAY_WORDS, 16) == 0x28e2076b);
	ok1(hash_stable(array, ARRAY_WORDS, 32) == 0xb73091c5);
	ok1(hash_stable(array, ARRAY_WORDS, 64) == 0x87daf5db);
	ok1(hash_stable(array, ARRAY_WORDS, 128) == 0xa16dfe20);
	ok1(hash_stable(array, ARRAY_WORDS, 256) == 0x300c63c3);
	ok1(hash_stable(array, ARRAY_WORDS, 512) == 0x255c91fc);
	ok1(hash_stable(array, ARRAY_WORDS, 1024) == 0x6357b26);
	ok1(hash_stable(array, ARRAY_WORDS, 2048) == 0x4bc5f339);
	ok1(hash_stable(array, ARRAY_WORDS, 4096) == 0x1301617c);
	ok1(hash_stable(array, ARRAY_WORDS, 8192) == 0x506792c9);
	ok1(hash_stable(array, ARRAY_WORDS, 16384) == 0xcd596705);
	ok1(hash_stable(array, ARRAY_WORDS, 32768) == 0xa8713cac);
	ok1(hash_stable(array, ARRAY_WORDS, 65536) == 0x94d9794);
	ok1(hash_stable(array, ARRAY_WORDS, 131072) == 0xac753e8);
	ok1(hash_stable(array, ARRAY_WORDS, 262144) == 0xcd8bdd20);
	ok1(hash_stable(array, ARRAY_WORDS, 524288) == 0xd44faf80);
	ok1(hash_stable(array, ARRAY_WORDS, 1048576) == 0x2547ccbe);
	ok1(hash_stable(array, ARRAY_WORDS, 2097152) == 0xbab06dbc);
	ok1(hash_stable(array, ARRAY_WORDS, 4194304) == 0xaac0e882);
	ok1(hash_stable(array, ARRAY_WORDS, 8388608) == 0x443f48d0);
	ok1(hash_stable(array, ARRAY_WORDS, 16777216) == 0xdff49fcc);
	ok1(hash_stable(array, ARRAY_WORDS, 33554432) == 0x9ce0fd65);
	ok1(hash_stable(array, ARRAY_WORDS, 67108864) == 0x9ddb1def);
	ok1(hash_stable(array, ARRAY_WORDS, 134217728) == 0x86096f25);
	ok1(hash_stable(array, ARRAY_WORDS, 268435456) == 0xe713b7b5);
	ok1(hash_stable(array, ARRAY_WORDS, 536870912) == 0x5baeffc5);
	ok1(hash_stable(array, ARRAY_WORDS, 1073741824) == 0xde874f52);
	ok1(hash_stable(array, ARRAY_WORDS, 2147483648U) == 0xeca13b4e);

	/* Hash should be the same, indep of memory alignment. */
	val = hash(array, sizeof(array), 0);
	for (i = 0; i < sizeof(uint32_t); i++) {
		memcpy(array2 + i, array, sizeof(array));
		ok(hash(array2 + i, sizeof(array), 0) != val,
		   "hash matched at offset %i", i);
	}

	/* Hash of random values should have random distribution:
	 * check one byte at a time. */
	for (i = 0; i < sizeof(uint32_t); i++) {
		unsigned int lowest = -1U, highest = 0;

		memset(results, 0, sizeof(results));

		for (j = 0; j < 256000; j++) {
			for (k = 0; k < ARRAY_WORDS; k++)
				array[k] = random();
			results[(hash(array, sizeof(array), 0) >> i*8)&0xFF]++;
		}

		for (j = 0; j < 256; j++) {
			if (results[j] < lowest)
				lowest = results[j];
			if (results[j] > highest)
				highest = results[j];
		}
		/* Expect within 20% */
		ok(lowest > 800, "Byte %i lowest %i", i, lowest);
		ok(highest < 1200, "Byte %i highest %i", i, highest);
		diag("Byte %i, range %u-%u", i, lowest, highest);
	}

	/* Hash of pointer values should also have random distribution. */
	for (i = 0; i < sizeof(uint32_t); i++) {
		unsigned int lowest = -1U, highest = 0;
		char *p = malloc(256000);

		memset(results, 0, sizeof(results));

		for (j = 0; j < 256000; j++)
			results[(hash_pointer(p + j, 0) >> i*8)&0xFF]++;
		free(p);

		for (j = 0; j < 256; j++) {
			if (results[j] < lowest)
				lowest = results[j];
			if (results[j] > highest)
				highest = results[j];
		}
		/* Expect within 20% */
		ok(lowest > 800, "hash_pointer byte %i lowest %i", i, lowest);
		ok(highest < 1200, "hash_pointer byte %i highest %i",
		   i, highest);
		diag("hash_pointer byte %i, range %u-%u", i, lowest, highest);
	}

	/* String hash: weak, so only test bottom byte */
	for (i = 0; i < 1; i++) {
		unsigned int num = 0, cursor, lowest = -1U, highest = 0;
		char p[5];

		memset(results, 0, sizeof(results));

		memset(p, 'A', sizeof(p));
		p[sizeof(p)-1] = '\0';

		for (;;) {
			for (cursor = 0; cursor < sizeof(p)-1; cursor++) {
				p[cursor]++;
				if (p[cursor] <= 'z')
					break;
				p[cursor] = 'A';
			}
			if (cursor == sizeof(p)-1)
				break;

			results[(hash_string(p) >> i*8)&0xFF]++;
			num++;
		}

		for (j = 0; j < 256; j++) {
			if (results[j] < lowest)
				lowest = results[j];
			if (results[j] > highest)
				highest = results[j];
		}
		/* Expect within 20% */
		ok(lowest > 35000, "hash_pointer byte %i lowest %i", i, lowest);
		ok(highest < 53000, "hash_pointer byte %i highest %i",
		   i, highest);
		diag("hash_pointer byte %i, range %u-%u", i, lowest, highest);
	}

	return exit_status();
}
