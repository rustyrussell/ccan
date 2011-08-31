/* We should be able to tdb1_check a O_RDONLY tdb, and we were previously allowed
 * to tdb1_check() inside a transaction (though that's paranoia!). */
#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB1_DATA key, data;

	plan_tests(11);
	tdb = tdb1_open_ex("run-readonly-check.tdb", 1024,
			  TDB1_DEFAULT,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb1_store(tdb, key, data, TDB1_INSERT) == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);

	/* We are also allowed to do a check inside a transaction. */
	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	ok1(tdb1_close(tdb) == 0);

	tdb = tdb1_open_ex("run-readonly-check.tdb", 1024,
			  TDB1_DEFAULT, O_RDONLY, 0, &taplogctx, NULL);

	ok1(tdb);
	ok1(tdb1_store(tdb, key, data, TDB1_MODIFY) == -1);
	ok1(tdb_error(tdb) == TDB_ERR_RDONLY);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	ok1(tdb1_close(tdb) == 0);

	return exit_status();
}
