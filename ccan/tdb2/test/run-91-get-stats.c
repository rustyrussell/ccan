#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		union tdb_attribute *attr;
		struct tdb_data key = tdb_mkdata("key", 3);

		tdb = tdb_open("run-91-get-stats.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		ok1(tdb_store(tdb, key, key, TDB_REPLACE) == 0);

		/* Use malloc so valgrind will catch overruns. */
		attr = malloc(sizeof *attr);
		attr->stats.base.attr = TDB_ATTRIBUTE_STATS;
		attr->stats.size = sizeof(*attr);

		ok1(tdb_get_attribute(tdb, attr) == 0);
		ok1(attr->stats.size == sizeof(*attr));
		ok1(attr->stats.allocs > 0);
		ok1(attr->stats.expands > 0);
		ok1(attr->stats.locks > 0);
		free(attr);

		/* Try short one. */
		attr = malloc(offsetof(struct tdb_attribute_stats, allocs)
			      + sizeof(attr->stats.allocs));
		attr->stats.base.attr = TDB_ATTRIBUTE_STATS;
		attr->stats.size = offsetof(struct tdb_attribute_stats, allocs)
			+ sizeof(attr->stats.allocs);
		ok1(tdb_get_attribute(tdb, attr) == 0);
		ok1(attr->stats.size == sizeof(*attr));
		ok1(attr->stats.allocs > 0);
		free(attr);
		ok1(tap_log_messages == 0);

		tdb_close(tdb);

	}
	return exit_status();
}
