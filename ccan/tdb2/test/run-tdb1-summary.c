#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };
	TDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	TDB_DATA data = { (unsigned char *)&j, sizeof(j) };
	char *summary;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 14);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb1_open("run-summary.tdb", 131, flags[i],
				O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Put some stuff in there. */
		for (j = 0; j < 500; j++) {
			/* Make sure padding varies to we get some graphs! */
			data.dsize = j % (sizeof(j) + 1);
			if (tdb1_store(tdb, key, data, TDB_REPLACE) != 0)
				fail("Storing in tdb");
		}

		summary = tdb1_summary(tdb);
		diag("%s", summary);
		ok1(strstr(summary, "Size of file/data: "));
		ok1(strstr(summary, "Number of records: 500\n"));
		ok1(strstr(summary, "Smallest/average/largest keys: 4/4/4\n"));
		ok1(strstr(summary, "Smallest/average/largest data: 0/2/4\n"));
		ok1(strstr(summary, "Smallest/average/largest padding: "));
		ok1(strstr(summary, "Number of dead records: 0\n"));
		ok1(strstr(summary, "Number of free records: 1\n"));
		ok1(strstr(summary, "Smallest/average/largest free records: "));
		ok1(strstr(summary, "Number of hash chains: 131\n"));
		ok1(strstr(summary, "Smallest/average/largest hash chains: "));
		ok1(strstr(summary, "Number of uncoalesced records: 0\n"));
		ok1(strstr(summary, "Smallest/average/largest uncoalesced runs: 0/0/0\n"));
		ok1(strstr(summary, "Percentage keys/data/padding/free/dead/rechdrs&tailers/hashes: "));

		free(summary);
		tdb1_close(tdb);
	}

	return exit_status();
}
