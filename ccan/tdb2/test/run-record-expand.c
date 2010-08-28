#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

#define MAX_SIZE 10000
#define SIZE_STEP 131

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT };
	struct tdb_data key = { (unsigned char *)"key", 3 };
	struct tdb_data data;

	data.dptr = malloc(MAX_SIZE);
	memset(data.dptr, 0x24, MAX_SIZE);

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (3 + (1 + (MAX_SIZE/SIZE_STEP)) * 2) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("/tmp/run-new_database.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		tdb->log = tap_log_fn;
		ok1(tdb);
		if (!tdb)
			continue;

		data.dsize = 0;
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		for (data.dsize = 0;
		     data.dsize < MAX_SIZE;
		     data.dsize += SIZE_STEP) {
			memset(data.dptr, data.dsize, data.dsize);
			ok1(tdb_store(tdb, key, data, TDB_MODIFY) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
		}
		tdb_close(tdb);
	}
	ok1(tap_log_messages == 0);
	return exit_status();
}
