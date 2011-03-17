#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include <err.h>
#include "logging.h"

static bool empty_freetable(struct tdb_context *tdb)
{
	struct tdb_freetable ftab;
	unsigned int i;

	/* Now, free table should be completely exhausted in zone 0 */
	if (tdb_read_convert(tdb, tdb->ftable_off, &ftab, sizeof(ftab)) != 0)
		abort();

	for (i = 0; i < sizeof(ftab.buckets)/sizeof(ftab.buckets[0]); i++) {
		if (ftab.buckets[i])
			return false;
	}
	return true;
}


int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 9 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		TDB_DATA k;
		uint64_t size;
		bool was_empty = false;

		k.dptr = (void *)&j;
		k.dsize = sizeof(j);

		tdb = tdb_open("run-30-exhaust-before-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(empty_freetable(tdb));
		/* Need some hash lock for expand. */
		ok1(tdb_lock_hashes(tdb, 0, 1, F_WRLCK, TDB_LOCK_WAIT) == 0);
		/* Create some free space. */
		ok1(tdb_expand(tdb, 1) == 0);
		ok1(tdb_unlock_hashes(tdb, 0, 1, F_WRLCK) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(!empty_freetable(tdb));

		size = tdb->file->map_size;
		/* Insert minimal-length records until we expand. */
		for (j = 0; tdb->file->map_size == size; j++) {
			was_empty = empty_freetable(tdb);
			if (tdb_store(tdb, k, k, TDB_INSERT) != 0)
				err(1, "Failed to store record %i", j);
		}

		/* Would have been empty before expansion, but no longer. */
		ok1(was_empty);
		ok1(!empty_freetable(tdb));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
