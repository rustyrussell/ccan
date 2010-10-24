#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tap/tap.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 5 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb_off_t free[BUCKETS_FOR_ZONE(INITIAL_ZONE_BITS) + 1];
		bool all_empty;
		TDB_DATA k, d;

		k.dptr = (void *)&j;
		k.dsize = sizeof(j);

		tdb = tdb_open("run-30-exhaust-before-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* We don't want the hash to expand, so we use one alloc to
		 * chew up over most of the space first. */
		j = -1;
		d.dsize = (1 << INITIAL_ZONE_BITS) - 500;
		d.dptr = malloc(d.dsize);
		ok1(tdb_store(tdb, k, d, TDB_INSERT) == 0);
		ok1(tdb->map_size == sizeof(struct tdb_header)
		    + (1 << INITIAL_ZONE_BITS)+1);

		/* Insert minimal-length records until we add a zone. */ 
		for (j = 0;
		     tdb->map_size == sizeof(struct tdb_header)
			     + (1 << INITIAL_ZONE_BITS)+1;
		     j++) {
			if (tdb_store(tdb, k, k, TDB_INSERT) != 0)
				err(1, "Failed to store record %i", j);
		}

		/* Now, free list should be completely exhausted in zone 0 */
		ok1(tdb_read_convert(tdb,
				     sizeof(struct tdb_header)
				     + sizeof(struct free_zone_header),
				     &free, sizeof(free)) == 0);

		all_empty = true;
		for (j = 0; j < sizeof(free)/sizeof(free[0]); j++) {
			if (free[j]) {
				diag("Free bucket %i not empty", j);
				all_empty = false;
			}
		}
		ok1(all_empty);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
