#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB1_DATA key, data;

	plan_tests(27);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";

	tdb = tdb1_open_ex("run-nested-transactions.tdb",
			  1024, TDB1_CLEAR_IF_FIRST|TDB1_DISALLOW_NESTING,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);
	ok1(tdb);

	ok1(tdb1_transaction_start(tdb) == 0);
	data.dptr = (void *)"world";
	data.dsize = strlen("world");
	ok1(tdb1_store(tdb, key, data, TDB1_INSERT) == 0);
	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == strlen("world"));
	ok1(memcmp(data.dptr, "world", strlen("world")) == 0);
	free(data.dptr);
	ok1(tdb1_transaction_start(tdb) != 0);
	ok1(tdb1_error(tdb) == TDB1_ERR_NESTING);

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

	/* Allow nesting by default. */
	tdb = tdb1_open_ex("run-nested-transactions.tdb",
			  1024, TDB1_DEFAULT, O_RDWR, 0, &taplogctx, NULL);
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
