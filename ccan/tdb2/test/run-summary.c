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
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = { (unsigned char *)&j, sizeof(j) };
	struct tdb_data data = { (unsigned char *)&j, sizeof(j) };
	char *summary;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (1 + 2 * 4) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-summary.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Put some stuff in there. */
		for (j = 0; j < 500; j++) {
			/* Make sure padding varies to we get some graphs! */
			data.dsize = j % (sizeof(j) + 1);
			if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
				fail("Storing in tdb");
		}

		for (j = 0;
		     j <= TDB_SUMMARY_HISTOGRAMS;
		     j += TDB_SUMMARY_HISTOGRAMS) {
			summary = tdb_summary(tdb, j);
			ok1(strstr(summary, "Number of records: 500\n"));
			ok1(strstr(summary, "Smallest/average/largest keys: 4/4/4\n"));
			ok1(strstr(summary, "Smallest/average/largest data: 0/2/4\n"));
			if (j == TDB_SUMMARY_HISTOGRAMS)
				ok1(strstr(summary, "|")
				    && strstr(summary, "*"));
			else
				ok1(!strstr(summary, "|")
				    && !strstr(summary, "*"));
			free(summary);
		}
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}


