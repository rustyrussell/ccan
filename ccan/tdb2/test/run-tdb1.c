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

	plan_tests(10);
	tdb = tdb1_open("run.tdb", TDB_DEFAULT,
			O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb1_store(tdb, key, data, TDB_MODIFY) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_NOEXIST);
	ok1(tdb1_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb1_store(tdb, key, data, TDB_INSERT) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_EXISTS);
	ok1(tdb1_store(tdb, key, data, TDB_MODIFY) == 0);

	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);

	key.dsize++;
	data = tdb1_fetch(tdb, key);
	ok1(data.dptr == NULL);
	tdb1_close(tdb);

	return exit_status();
}
