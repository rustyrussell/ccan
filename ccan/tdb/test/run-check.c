#define _XOPEN_SOURCE 500
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

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(13);
	tdb = tdb_open_ex("run-check.tdb", 1, TDB_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("run-check.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("test/tdb.corrupt", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == -1);
	ok1(tdb_error(tdb) == TDB_ERR_CORRUPT);
	tdb_close(tdb);

	/* Big and little endian should work! */
	tdb = tdb_open_ex("test/old-nohash-le.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("test/old-nohash-be.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	return exit_status();
}
