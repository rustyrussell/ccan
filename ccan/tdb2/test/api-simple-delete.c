#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-simple-delete.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (tdb) {
			/* Delete should fail. */
			ok1(tdb_delete(tdb, key) == TDB_ERR_NOEXIST);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			/* Insert should succeed. */
			ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			/* Delete should now work. */
			ok1(tdb_delete(tdb, key) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			tdb_close(tdb);
		}
	}
	ok1(tap_log_messages == 0);
	return exit_status();
}
