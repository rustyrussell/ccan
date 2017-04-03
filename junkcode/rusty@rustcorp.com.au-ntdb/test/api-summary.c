#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	NTDB_DATA data = { (unsigned char *)&j, sizeof(j) };
	char *summary;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (1 + 2 * 5) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-summary.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Put some stuff in there. */
		for (j = 0; j < 500; j++) {
			/* Make sure padding varies to we get some graphs! */
			data.dsize = j % (sizeof(j) + 1);
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
				fail("Storing in ntdb");
		}

		for (j = 0;
		     j <= NTDB_SUMMARY_HISTOGRAMS;
		     j += NTDB_SUMMARY_HISTOGRAMS) {
			ok1(ntdb_summary(ntdb, j, &summary) == NTDB_SUCCESS);
			ok1(strstr(summary, "Number of records: 500\n"));
			ok1(strstr(summary, "Smallest/average/largest keys: 4/4/4\n"));
			ok1(strstr(summary, "Smallest/average/largest data: 0/2/4\n"));
			if (j == NTDB_SUMMARY_HISTOGRAMS) {
				ok1(strstr(summary, "|")
				    && strstr(summary, "*"));
			} else {
				ok1(!strstr(summary, "|")
				    && !strstr(summary, "*"));
			}
			free(summary);
		}
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
