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

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 18 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* First expand. Should not fill zone. */
		val = tdb->map_size - sizeof(struct tdb_header);
		ok1(tdb_expand(tdb, 1) == 0);
		ok1(tdb->map_size < sizeof(struct tdb_header)
		    + (1 << INITIAL_ZONE_BITS));
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Fill zone. */
		val = (1<<INITIAL_ZONE_BITS)
			- sizeof(struct tdb_used_record)
			- (tdb->map_size - sizeof(struct tdb_header));
		ok1(tdb_expand(tdb, val) == 0);
		ok1(tdb->map_size == sizeof(struct tdb_header)
		    + (1 << INITIAL_ZONE_BITS));
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Second expand, adds another zone of same size. */
		ok1(tdb_expand(tdb, 4 << INITIAL_ZONE_BITS) == 0);
		ok1(tdb->map_size ==
		    (2<<INITIAL_ZONE_BITS) + sizeof(struct tdb_header));
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Large expand now will double file. */
		ok1(tdb_expand(tdb, 4 << INITIAL_ZONE_BITS) == 0);
		ok1(tdb->map_size ==
		    (4<<INITIAL_ZONE_BITS) + sizeof(struct tdb_header));
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* And again? */
		ok1(tdb_expand(tdb, 4 << INITIAL_ZONE_BITS) == 0);
		ok1(tdb->map_size ==
		    (8<<INITIAL_ZONE_BITS) + sizeof(struct tdb_header));
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Below comfort level, won't fill zone. */
		ok1(tdb_expand(tdb,
			       ((3 << INITIAL_ZONE_BITS)
				>> TDB_COMFORT_FACTOR_BITS)
			       - sizeof(struct tdb_used_record)) == 0);
		ok1(tdb->map_size < (12<<INITIAL_ZONE_BITS)
		    + sizeof(struct tdb_header));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
