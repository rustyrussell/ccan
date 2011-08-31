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

	plan_tests(27);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";

	tdb = tdb_open("run-nested-transactions.tdb1",
		       TDB_VERSION1, O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);
	ok1(tdb);

	/* No nesting by default. */
	ok1(tdb1_transaction_start(tdb) == 0);
	data.dptr = (void *)"world";
	data.dsize = strlen("world");
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb1_transaction_start(tdb) != 0);
	ok1(tdb_error(tdb) == TDB_ERR_EINVAL);

	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb1_transaction_commit(tdb) == 0);
	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	tdb_close(tdb);

	tdb = tdb_open("run-nested-transactions.tdb1",
		       TDB_ALLOW_NESTING, O_RDWR, 0, &tap_log_attr);
	ok1(tdb);

	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_delete(tdb, key) == 0);
	ok1(tdb1_transaction_commit(tdb) == 0);
	ok1(!tdb1_exists(tdb, key));
	ok1(tdb1_transaction_cancel(tdb) == 0);
	/* Surprise! Kills inner "committed" transaction. */
	ok1(tdb1_exists(tdb, key));

	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_delete(tdb, key) == 0);
	ok1(tdb1_transaction_commit(tdb) == 0);
	ok1(!tdb1_exists(tdb, key));
	ok1(tdb1_transaction_commit(tdb) == 0);
	ok1(!tdb1_exists(tdb, key));
	tdb_close(tdb);

	return exit_status();
}
