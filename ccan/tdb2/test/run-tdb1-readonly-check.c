/* We should be able to tdb1_check a O_RDONLY tdb, and we were previously allowed
 * to tdb1_check() inside a transaction (though that's paranoia!). */
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
	tdb = tdb_open("run-readonly-check.tdb1",
		       TDB_VERSION1,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);

	ok1(tdb);
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dsize = strlen("world");
	data.dptr = (void *)"world";

	ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);

	/* We are also allowed to do a check inside a transaction. */
	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	ok1(tdb_close(tdb) == 0);

	tdb = tdb_open("run-readonly-check.tdb1",
		       TDB_DEFAULT, O_RDONLY, 0, &tap_log_attr);

	ok1(tdb);
	ok1(tdb_store(tdb, key, data, TDB_MODIFY) == TDB_ERR_RDONLY);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	ok1(tdb_close(tdb) == 0);

	return exit_status();
}
