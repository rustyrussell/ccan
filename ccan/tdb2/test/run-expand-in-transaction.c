#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
			TDB_CONVERT|TDB_NOSYNC,
			TDB_NOMMAP|TDB_CONVERT|TDB_NOSYNC };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t size;
		tdb = tdb_open("run-expand-in-transaction.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		size = tdb->file->map_size;
		ok1(tdb_transaction_start(tdb) == 0);
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb->file->map_size > size);
		ok1(tdb_transaction_commit(tdb) == 0);
		ok1(tdb->file->map_size > size);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
