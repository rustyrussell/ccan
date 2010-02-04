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
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(13);
	tdb = tdb_open("/tmp/test.tdb", 1024, TDB_CLEAR_IF_FIRST|TDB_CONVERT,
		       O_CREAT|O_TRUNC|O_RDWR, 0600);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, data, TDB_MODIFY) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_NOEXIST);
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_store(tdb, key, data, TDB_INSERT) < 0);
	ok1(tdb_error(tdb) == TDB_ERR_EXISTS);
	ok1(tdb_store(tdb, key, data, TDB_MODIFY) == 0);

	data = tdb_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);

	key.dsize++;
	data = tdb_fetch(tdb, key);
	ok1(data.dptr == NULL);
	tdb_close(tdb);

	/* Reopen: should read it */
	tdb = tdb_open("/tmp/test.tdb", 1024, 0, O_RDWR, 0);
	ok1(tdb);
	
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data = tdb_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	tdb_close(tdb);

	return exit_status();
}
