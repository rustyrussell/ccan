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

	plan_tests(27);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";

	tdb = tdb1_open("run-nested-transactions.tdb",
			1024, TDB_DEFAULT,
			O_CREAT|O_TRUNC|O_RDWR, 0600, &tap_log_attr);
	ok1(tdb);

	/* No nesting by default. */
	ok1(tdb1_transaction_start(tdb) == 0);
	data.dptr = (void *)"world";
	data.dsize = strlen("world");
	ok1(tdb1_store(tdb, key, data, TDB_INSERT) == 0);
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
	tdb1_close(tdb);

	tdb = tdb1_open("run-nested-transactions.tdb",
			1024, TDB_ALLOW_NESTING, O_RDWR, 0, &tap_log_attr);
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
	tdb1_close(tdb);

	return exit_status();
}
