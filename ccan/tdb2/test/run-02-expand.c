#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	uint64_t val;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		val = tdb->map_size;
		ok1(tdb_expand(tdb, 1) == 0);
		ok1(tdb->map_size >= val + 1 * TDB_EXTENSION_FACTOR);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		val = tdb->map_size;
		ok1(tdb_expand(tdb, 1024) == 0);
		ok1(tdb->map_size >= val + 1024 * TDB_EXTENSION_FACTOR);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
