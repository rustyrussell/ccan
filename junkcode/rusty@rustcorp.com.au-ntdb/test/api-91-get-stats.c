#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		union ntdb_attribute *attr;
		NTDB_DATA key = ntdb_mkdata("key", 3), data;

		ntdb = ntdb_open("run-91-get-stats.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		/* Force an expansion */
		data.dsize = 65536;
		data.dptr = calloc(data.dsize, 1);
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
		free(data.dptr);

		/* Use malloc so valgrind will catch overruns. */
		attr = malloc(sizeof *attr);
		attr->stats.base.attr = NTDB_ATTRIBUTE_STATS;
		attr->stats.size = sizeof(*attr);

		ok1(ntdb_get_attribute(ntdb, attr) == 0);
		ok1(attr->stats.size == sizeof(*attr));
		ok1(attr->stats.allocs > 0);
		ok1(attr->stats.expands > 0);
		ok1(attr->stats.locks > 0);
		free(attr);

		/* Try short one. */
		attr = malloc(offsetof(struct ntdb_attribute_stats, allocs)
			      + sizeof(attr->stats.allocs));
		attr->stats.base.attr = NTDB_ATTRIBUTE_STATS;
		attr->stats.size = offsetof(struct ntdb_attribute_stats, allocs)
			+ sizeof(attr->stats.allocs);
		ok1(ntdb_get_attribute(ntdb, attr) == 0);
		ok1(attr->stats.size == sizeof(*attr));
		ok1(attr->stats.allocs > 0);
		free(attr);
		ok1(tap_log_messages == 0);

		ntdb_close(ntdb);

	}
	return exit_status();
}
