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
#include <ccan/tdb/hash.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;

	plan_tests(8);

	/* Old format (with zeroes in the hash magic fields) should
	 * open with any hash (since we don't know what hash they used). */
	tdb = tdb_open_ex("test/old-nohash-le.tdb", 0, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("test/old-nohash-be.tdb", 0, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("test/old-nohash-le.tdb", 0, 0, O_RDWR, 0,
			  &taplogctx, tdb_jenkins_hash);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	tdb = tdb_open_ex("test/old-nohash-be.tdb", 0, 0, O_RDWR, 0,
			  &taplogctx, tdb_jenkins_hash);
	ok1(tdb);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	tdb_close(tdb);

	return exit_status();
}
