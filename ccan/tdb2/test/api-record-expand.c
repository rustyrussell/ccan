#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "logging.h"

#define MAX_SIZE 10000
#define SIZE_STEP 131

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
	struct tdb_data data;

	data.dptr = malloc(MAX_SIZE);
	memset(data.dptr, 0x24, MAX_SIZE);

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (3 + (1 + (MAX_SIZE/SIZE_STEP)) * 2) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-record-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
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
	free(data.dptr);

	return exit_status();
}
