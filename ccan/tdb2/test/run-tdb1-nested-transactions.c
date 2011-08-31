#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
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

	plan_tests(29);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";

	tdb = tdb_open("run-nested-transactions.tdb1",
		       TDB_VERSION1, O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);
	ok1(tdb);

	/* No nesting by default. */
	ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
	data.dptr = (void *)"world";
	data.dsize = strlen("world");
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
	ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb_transaction_start(tdb) == TDB_ERR_EINVAL);

	ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
	ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	tdb_close(tdb);

	tdb = tdb_open("run-nested-transactions.tdb1",
		       TDB_ALLOW_NESTING, O_RDWR, 0, &tap_log_attr);
	ok1(tdb);

	ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
	ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
	ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
	ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
	ok1(!tdb_exists(tdb, key));
	tdb_transaction_cancel(tdb);
	ok1(tap_log_messages == 0);
	/* Surprise! Kills inner "committed" transaction. */
	ok1(tdb_exists(tdb, key));

	ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
	ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
	ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
	ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
	ok1(!tdb_exists(tdb, key));
	ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
	ok1(!tdb_exists(tdb, key));
	tdb_close(tdb);

	return exit_status();
}
