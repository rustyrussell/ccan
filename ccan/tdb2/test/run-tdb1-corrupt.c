#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

static int check(TDB1_DATA key, TDB1_DATA data, void *private)
{
	unsigned int *sizes = private;

	if (key.dsize > strlen("hello"))
		return -1;
	if (memcmp(key.dptr, "hello", key.dsize) != 0)
		return -1;

	if (data.dsize != strlen("world"))
		return -1;
	if (memcmp(data.dptr, "world", data.dsize) != 0)
		return -1;

	sizes[0] += key.dsize;
	sizes[1] += data.dsize;
	return 0;
}

static void tdb1_flip_bit(struct tdb1_context *tdb, unsigned int bit)
{
	unsigned int off = bit / CHAR_BIT;
	unsigned char mask = (1 << (bit % CHAR_BIT));

	if (tdb->map_ptr)
		((unsigned char *)tdb->map_ptr)[off] ^= mask;
	else {
		unsigned char c;
		if (pread(tdb->fd, &c, 1, off) != 1)
			err(1, "pread");
		c ^= mask;
		if (pwrite(tdb->fd, &c, 1, off) != 1)
			err(1, "pwrite");
	}
}

static void check_test(struct tdb1_context *tdb)
{
	TDB1_DATA key, data;
	unsigned int i, verifiable, corrupt, sizes[2], dsize, ksize;

	ok1(tdb1_check(tdb, NULL, NULL) == 0);

	key.dptr = (void *)"hello";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	/* Key and data size respectively. */
	dsize = ksize = 0;

	/* 5 keys in hash size 2 means we'll have multichains. */
	for (key.dsize = 1; key.dsize <= 5; key.dsize++) {
		ksize += key.dsize;
		dsize += data.dsize;
		if (tdb1_store(tdb, key, data, TDB1_INSERT) != 0)
			abort();
	}

	/* This is how many bytes we expect to be verifiable. */
	/* From the file header. */
	verifiable = strlen(TDB1_MAGIC_FOOD) + 1
		+ 2 * sizeof(uint32_t) + 2 * sizeof(tdb1_off_t)
		+ 2 * sizeof(uint32_t);
	/* From the free list chain and hash chains. */
	verifiable += 3 * sizeof(tdb1_off_t);
	/* From the record headers & tailer */
	verifiable += 5 * (sizeof(struct tdb1_record) + sizeof(uint32_t));
	/* The free block: we ignore datalen, keylen, full_hash. */
	verifiable += sizeof(struct tdb1_record) - 3*sizeof(uint32_t) +
		sizeof(uint32_t);
	/* Our check function verifies the key and data. */
	verifiable += ksize + dsize;

	/* Flip one bit at a time, make sure it detects verifiable bytes. */
	for (i = 0, corrupt = 0; i < tdb->map_size * CHAR_BIT; i++) {
		tdb1_flip_bit(tdb, i);
		memset(sizes, 0, sizeof(sizes));
		if (tdb1_check(tdb, check, sizes) != 0)
			corrupt++;
		else if (sizes[0] != ksize || sizes[1] != dsize)
			corrupt++;
		tdb1_flip_bit(tdb, i);
	}
	ok(corrupt == verifiable * CHAR_BIT, "corrupt %u should be %u",
	   corrupt, verifiable * CHAR_BIT);
}

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;

	plan_tests(4);
	/* This should use mmap. */
	tdb = tdb1_open_ex("run-corrupt.tdb", 2, TDB1_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	if (!tdb)
		abort();
	check_test(tdb);
	tdb1_close(tdb);

	/* This should not. */
	tdb = tdb1_open_ex("run-corrupt.tdb", 2, TDB1_CLEAR_IF_FIRST|TDB1_NOMMAP,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	if (!tdb)
		abort();
	check_test(tdb);
	tdb1_close(tdb);

	return exit_status();
}
