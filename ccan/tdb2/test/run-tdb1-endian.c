#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB1_DATA key, data;

	plan_tests(13);
	tdb = tdb1_open_ex("run-endian.tdb", 1024,
			  TDB1_CLEAR_IF_FIRST|TDB1_CONVERT,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb1_store(tdb, key, data, TDB1_MODIFY) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_NOEXIST);
	ok1(tdb1_store(tdb, key, data, TDB1_INSERT) == 0);
	ok1(tdb1_store(tdb, key, data, TDB1_INSERT) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_EXISTS);
	ok1(tdb1_store(tdb, key, data, TDB1_MODIFY) == 0);

	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);

	key.dsize++;
	data = tdb1_fetch(tdb, key);
	ok1(data.dptr == NULL);
	tdb1_close(tdb);

	/* Reopen: should read it */
	tdb = tdb1_open_ex("run-endian.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	tdb1_close(tdb);

	return exit_status();
}
