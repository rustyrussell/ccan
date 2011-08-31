#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB_DATA key, data;

	plan_tests(13);
	tdb = tdb1_open_ex("run-check.tdb", 1, TDB1_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb1_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("run-check.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("test/tdb1.corrupt", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == -1);
	ok1(tdb_error(tdb) == TDB_ERR_CORRUPT);
	tdb1_close(tdb);

	/* Big and little endian should work! */
	tdb = tdb1_open_ex("test/old-nohash-le.tdb1", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("test/old-nohash-be.tdb1", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	return exit_status();
}
