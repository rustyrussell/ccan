#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/summary.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = { (unsigned char *)&j, sizeof(j) };
	struct tdb_data data = { (unsigned char *)&j, sizeof(j) };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 8 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		uint64_t features;
		tdb = tdb_open("run-features.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Put some stuff in there. */
		for (j = 0; j < 100; j++) {
			if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
				fail("Storing in tdb");
		}

		/* Mess with features fields in hdr. */
		features = (~TDB_FEATURE_MASK ^ 1);
		ok1(tdb_write_convert(tdb, offsetof(struct tdb_header,
						    features_used), 
				      &features, sizeof(features)) == 0);
		ok1(tdb_write_convert(tdb, offsetof(struct tdb_header,
						    features_offered), 
				      &features, sizeof(features)) == 0);
		tdb_close(tdb);

		tdb = tdb_open("run-features.tdb", flags[i], O_RDWR, 0,
			       &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Should not have changed features offered. */
		ok1(tdb_read_convert(tdb, offsetof(struct tdb_header,
						   features_offered), 
				     &features, sizeof(features)) == 0);
		ok1(features == (~TDB_FEATURE_MASK ^ 1));

		/* Should have cleared unknown bits in features_used. */
		ok1(tdb_read_convert(tdb, offsetof(struct tdb_header,
						   features_used), 
				     &features, sizeof(features)) == 0);
		ok1(features == (1 & TDB_FEATURE_MASK));

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}


