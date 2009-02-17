#include "crcsync/crcsync.h"
#include "crcsync/crcsync.c"
#include "tap/tap.h"
#include <stdlib.h>
#include <stdbool.h>

/* FIXME: ccanize. */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static void test_roll(unsigned int wsize)
{
	uint8_t data[wsize * 2];
	uint32_t uncrc_tab[256];
	unsigned int i;

	init_uncrc_tab(uncrc_tab, wsize);

	for (i = 0; i < ARRAY_SIZE(data); i++)
		data[i] = random();

	for (i = 1; i < ARRAY_SIZE(data) - wsize; i++) {
		uint32_t rollcrc, crc;

		crc = crc32c(0, data+i, wsize);
		rollcrc = crc_roll(crc32c(0, data+i-1, wsize),
				   data[i-1], data[i+wsize-1], uncrc_tab);

		ok(crc == rollcrc, "wsize %u, i %u", wsize, i);
	}
}

int main(int argc, char *argv[])
{
	plan_tests(100 - 1 + 128 - 1);
	test_roll(100);
	test_roll(128);
	return exit_status();
}
