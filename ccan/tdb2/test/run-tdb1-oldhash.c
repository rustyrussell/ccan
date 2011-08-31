#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;

	plan_tests(8);

	/* Old format (with zeroes in the hash magic fields) should
	 * open with any hash (since we don't know what hash they used). */
	tdb = tdb1_open_ex("test/old-nohash-le.tdb1", 0, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("test/old-nohash-be.tdb1", 0, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("test/old-nohash-le.tdb1", 0, 0, O_RDWR, 0,
			  &taplogctx, tdb1_incompatible_hash);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open_ex("test/old-nohash-be.tdb1", 0, 0, O_RDWR, 0,
			  &taplogctx, tdb1_incompatible_hash);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	return exit_status();
}
