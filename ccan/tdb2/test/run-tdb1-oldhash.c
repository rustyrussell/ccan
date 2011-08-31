#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	union tdb_attribute incompat_hash_attr;

	incompat_hash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	incompat_hash_attr.base.next = &tap_log_attr;
	incompat_hash_attr.hash.fn = tdb1_incompatible_hash;

	plan_tests(8);

	/* Old format (with zeroes in the hash magic fields) should
	 * open with any hash (since we don't know what hash they used). */
	tdb = tdb1_open("test/old-nohash-le.tdb1", 0, O_RDWR, 0,
			&tap_log_attr);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open("test/old-nohash-be.tdb1", 0, O_RDWR, 0,
			&tap_log_attr);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open("test/old-nohash-le.tdb1", 0, O_RDWR, 0,
			&incompat_hash_attr);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	tdb = tdb1_open("test/old-nohash-be.tdb1", 0, O_RDWR, 0,
			&incompat_hash_attr);
	ok1(tdb);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	return exit_status();
}
