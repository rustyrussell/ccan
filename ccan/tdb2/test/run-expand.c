#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

/* Release lock to check db. */
static void check(struct tdb_context *tdb)
{
	tdb_allrecord_unlock(tdb, F_WRLCK);
	ok1(tdb_check(tdb, NULL, NULL) == 0);
	ok1(tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false) == 0);
}

int main(int argc, char *argv[])
{
	unsigned int i;
	tdb_off_t off;
	uint64_t val, buckets;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 40 + 1);

	/* First, lower level expansion tests. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_allrecord_lock(tdb, F_WRLCK, TDB_LOCK_WAIT, false)
		    == 0);

		/* Expanding file is pretty easy. */
		off = expand_to_fill_zones(tdb);
		ok1(off > 0 && off != TDB_OFF_ERR);
		check(tdb);

		/* Second expand should do nothing. */
		ok1(expand_to_fill_zones(tdb) == 0);
		check(tdb);

		/* Now, try adding a zone. */
		val = tdb->header.v.num_zones + 1;
		ok1(update_zones(tdb, val,
				 tdb->header.v.zone_bits,
				 tdb->header.v.free_buckets,
				 1ULL << tdb->header.v.zone_bits) == 0);
		ok1(tdb->header.v.num_zones == val);
		check(tdb);

		/* Now, try doubling zone size. */
		val = tdb->header.v.zone_bits + 1;
		ok1(update_zones(tdb, tdb->header.v.num_zones,
				 val,
				 tdb->header.v.free_buckets,
				 1ULL << val) == 0);
		ok1(tdb->header.v.zone_bits == val);
		check(tdb);

		/* Now, try adding a zone, and a bucket. */
		val = tdb->header.v.num_zones + 1;
		buckets = tdb->header.v.free_buckets + 1;
		ok1(update_zones(tdb, val,
				 tdb->header.v.zone_bits,
				 buckets,
				 1ULL << tdb->header.v.zone_bits) == 0);
		ok1(tdb->header.v.num_zones == val);
		ok1(tdb->header.v.free_buckets == buckets);
		check(tdb);

		/* Now, try doubling zone size, and adding a bucket. */
		val = tdb->header.v.zone_bits + 1;
		buckets = tdb->header.v.free_buckets + 1;
		ok1(update_zones(tdb, tdb->header.v.num_zones,
				 val,
				 buckets,
				 1ULL << val) == 0);
		ok1(tdb->header.v.zone_bits == val);
		ok1(tdb->header.v.free_buckets == buckets);
		check(tdb);

		/* Now, try massive zone increase. */
		val = tdb->header.v.zone_bits + 4;
		ok1(update_zones(tdb, tdb->header.v.num_zones,
				 val,
				 tdb->header.v.free_buckets,
				 1ULL << val) == 0);
		ok1(tdb->header.v.zone_bits == val);
		check(tdb);

		tdb_allrecord_unlock(tdb, F_WRLCK);
		tdb_close(tdb);
	}

	/* Now using tdb_expand. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* First expand (expand file to fill zone). */
		ok1(tdb_expand(tdb, 1, 1, false) == 0);
		ok1(tdb->header.v.num_zones == 1);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		/* Little expand (extra zone). */
		ok1(tdb_expand(tdb, 1, 1, false) == 0);
		ok1(tdb->header.v.num_zones == 2);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		/* Big expand (enlarge zones) */
		ok1(tdb_expand(tdb, 1, 4096, false) == 0);
		ok1(tdb->header.v.num_zones == 2);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
