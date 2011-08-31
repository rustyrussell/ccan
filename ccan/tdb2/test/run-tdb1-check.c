#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, data;
	union tdb_attribute hsize;

	hsize.base.attr = TDB_ATTRIBUTE_TDB1_HASHSIZE;
	hsize.base.next = &tap_log_attr;
	hsize.tdb1_hashsize.hsize = 1;

	plan_tests(13);
	tdb = tdb_open("run-check.tdb1", TDB_VERSION1,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);

	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	tdb_close(tdb);

	tdb = tdb_open("run-check.tdb1", TDB_VERSION1, O_RDWR, 0, &tap_log_attr);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	tdb_close(tdb);

	tdb = tdb_open("test/tdb1.corrupt", TDB_VERSION1, O_RDWR, 0,
			&tap_log_attr);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_ERR_CORRUPT);
	ok1(tdb_error(tdb) == TDB_ERR_CORRUPT);
	tdb_close(tdb);

	/* Big and little endian should work! */
	tdb = tdb_open("test/old-nohash-le.tdb1", TDB_VERSION1, O_RDWR, 0,
		       &tap_log_attr);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	tdb_close(tdb);

	tdb = tdb_open("test/old-nohash-be.tdb1", TDB_VERSION1, O_RDWR, 0,
			&tap_log_attr);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
	tdb_close(tdb);

	return exit_status();
}
