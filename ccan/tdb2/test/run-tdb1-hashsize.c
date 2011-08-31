#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	union tdb_attribute hsize, h2;

	hsize.base.attr = TDB_ATTRIBUTE_TDB1_HASHSIZE;
	hsize.base.next = &tap_log_attr;
	hsize.tdb1_hashsize.hsize = 1024;

	plan_tests(14);
	tdb = tdb_open("run-tdb1-hashsize.tdb1", TDB_VERSION1,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);
	ok1(tdb);
	h2.base.attr = TDB_ATTRIBUTE_TDB1_HASHSIZE;
	ok1(tdb_get_attribute(tdb, &h2) == TDB_SUCCESS);
	ok1(h2.tdb1_hashsize.hsize == hsize.tdb1_hashsize.hsize);
	tdb_close(tdb);

	/* Can't specify TDB_ATTRIBUTE_TDB1_HASHSIZE without O_CREAT */
	tdb = tdb_open("run-tdb1-hashsize.tdb1", TDB_VERSION1,
		       O_RDWR, 0600, &hsize);
	ok1(!tdb);
	ok1(tap_log_messages == 1);

	/* Can't specify TDB_ATTRIBUTE_TDB1_HASHSIZE for version2. */
	tdb = tdb_open("run-tdb1-hashsize.tdb", TDB_DEFAULT,
			O_CREAT|O_TRUNC|O_RDWR, 0600, &hsize);
	ok1(!tdb);
	ok1(tap_log_messages == 2);

	/* We can get attribute even if we didn't set it though. */
	tdb = tdb_open("run-tdb1-hashsize.tdb1", TDB_DEFAULT,
		       O_RDWR, 0600, &tap_log_attr);

	ok1(tdb);
	memset(&h2, 0, sizeof(h2));
	h2.base.attr = TDB_ATTRIBUTE_TDB1_HASHSIZE;
	ok1(tdb_get_attribute(tdb, &h2) == TDB_SUCCESS);
	ok1(h2.tdb1_hashsize.hsize == hsize.tdb1_hashsize.hsize);
	tdb_close(tdb);

	/* Check for default hash size. */
	tdb = tdb_open("run-tdb1-hashsize.tdb1", TDB_VERSION1,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &tap_log_attr);

	ok1(tdb);
	memset(&h2, 0, sizeof(h2));
	h2.base.attr = TDB_ATTRIBUTE_TDB1_HASHSIZE;
	ok1(tdb_get_attribute(tdb, &h2) == TDB_SUCCESS);
	ok1(h2.tdb1_hashsize.hsize == TDB1_DEFAULT_HASH_SIZE);
	tdb_close(tdb);
	ok1(tap_log_messages == 2);

	return exit_status();
}
