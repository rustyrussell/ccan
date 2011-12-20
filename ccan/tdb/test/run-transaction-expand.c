/* We need this otherwise fcntl locking fails. */
#define _FILE_OFFSET_BITS 64
#define _XOPEN_SOURCE 500
#include <ccan/tdb/tdb_private.h>

/* Speed up the tests: setting TDB_NOSYNC removed recovery altogether. */
static inline int fake_fsync(int fd)
{
	return 0;
}
#define fsync fake_fsync

#ifdef MS_SYNC
static inline int fake_msync(void *addr, size_t length, int flags)
{
	return 0;
}
#define msync fake_msync
#endif

#include <ccan/tdb/tdb.h>
#include <ccan/tdb/io.c>
#include <ccan/tdb/tdb.c>
#include <ccan/tdb/lock.c>
#include <ccan/tdb/freelist.c>
#include <ccan/tdb/traverse.c>
#include <ccan/tdb/transaction.c>
#include <ccan/tdb/error.c>
#include <ccan/tdb/open.c>
#include <ccan/tdb/check.c>
#include <ccan/tdb/hash.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

static void write_record(struct tdb_context *tdb, size_t extra_len,
			 TDB_DATA *data)
{
	TDB_DATA key;
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";

	data->dsize += extra_len;
	tdb_transaction_start(tdb);
	tdb_store(tdb, key, *data, TDB_REPLACE);
	tdb_transaction_commit(tdb);
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	size_t i;
	TDB_DATA data;
	struct tdb_record rec;
	tdb_off_t off;

	plan_tests(4);
	tdb = tdb_open_ex("run-transaction-expand.tdb",
			  1024, TDB_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);
	ok1(tdb);

	data.dsize = 0;
	data.dptr = calloc(1000, getpagesize());

	/* Simulate a slowly growing record. */
	for (i = 0; i < 1000; i++)
		write_record(tdb, getpagesize(), &data);

	tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &off);
	tdb_read(tdb, off, &rec, sizeof(rec), DOCONV());
	diag("TDB size = %zu, recovery = %u-%u",
	     (size_t)tdb->map_size, off, off + sizeof(rec) + rec.rec_len);

	/* We should only be about 5 times larger than largest record. */
	ok1(tdb->map_size < 6 * i * getpagesize());
	tdb_close(tdb);

	tdb = tdb_open_ex("run-transaction-expand.tdb",
			  1024, TDB_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);
	ok1(tdb);

	data.dsize = 0;

	/* Simulate a slowly growing record, repacking to keep
	 * recovery area at end. */
	for (i = 0; i < 1000; i++) {
		write_record(tdb, getpagesize(), &data);
		if (i % 10 == 0)
			tdb_repack(tdb);
	}

	tdb_ofs_read(tdb, TDB_RECOVERY_HEAD, &off);
	tdb_read(tdb, off, &rec, sizeof(rec), DOCONV());
	diag("TDB size = %zu, recovery = %u-%u",
	     (size_t)tdb->map_size, off, off + sizeof(rec) + rec.rec_len);

	/* We should only be about 4 times larger than largest record. */
	ok1(tdb->map_size < 5 * i * getpagesize());
	tdb_close(tdb);
	free(data.dptr);

	return exit_status();
}
