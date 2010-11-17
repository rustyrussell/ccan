#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tap/tap.h>
#include <err.h>
#include "logging.h"

static bool empty_freelist(struct tdb_context *tdb)
{
	struct tdb_freelist free;
	unsigned int i;

	/* Now, free list should be completely exhausted in zone 0 */
	if (tdb_read_convert(tdb, tdb->flist_off, &free, sizeof(free)) != 0)
		abort();

	for (i = 0; i < sizeof(free.buckets)/sizeof(free.buckets[0]); i++) {
		if (free.buckets[i])
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

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);

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

		ok1(empty_freelist(tdb));
		/* Create some free space. */
		ok1(tdb_expand(tdb, 1) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(!empty_freelist(tdb));

		size = tdb->map_size;
		/* Insert minimal-length records until we expand. */
		for (j = 0; tdb->map_size == size; j++) {
			was_empty = empty_freelist(tdb);
			if (tdb_store(tdb, k, k, TDB_INSERT) != 0)
				err(1, "Failed to store record %i", j);
		}

		/* Would have been empty before expansion, but no longer. */
		ok1(was_empty);
		ok1(!empty_freelist(tdb));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
