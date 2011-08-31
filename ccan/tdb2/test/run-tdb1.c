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
	hsize.tdb1_hashsize.hsize = 1024;

	plan_tests(9);
	tdb = tdb_open("run.tdb1", TDB_VERSION1,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, data, TDB_MODIFY) == TDB_ERR_NOEXIST);
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_ERR_EXISTS);
	ok1(tdb_store(tdb, key, data, TDB_MODIFY) == TDB_SUCCESS);

	ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);

	key.dsize++;
	ok1(tdb_fetch(tdb, key, &data) == TDB_ERR_NOEXIST);
	tdb_close(tdb);

	return exit_status();
}
