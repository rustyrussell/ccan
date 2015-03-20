#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "helprun-external-agent.h"

static int drop_count(struct ntdb_context *ntdb, unsigned int *count)
{
	if (--(*count) == 0)
		return 1;
	return 0;
}

static int set_found(struct ntdb_context *ntdb, bool found[3])
{
	unsigned int idx;

	if (strcmp(ntdb_name(ntdb), "run-ntdb_foreach0.ntdb") == 0)
		idx = 0;
	else if (strcmp(ntdb_name(ntdb), "run-ntdb_foreach1.ntdb") == 0)
		idx = 1;
	else if (strcmp(ntdb_name(ntdb), "run-ntdb_foreach2.ntdb") == 0)
		idx = 2;
	else
		abort();

	if (found[idx])
		abort();
	found[idx] = true;
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i, count;
	bool found[3];
	struct ntdb_context *ntdb0, *ntdb1, *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 8);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb0 = ntdb_open("run-ntdb_foreach0.ntdb",
				  flags[i]|MAYBE_NOSYNC,
				  O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ntdb1 = ntdb_open("run-ntdb_foreach1.ntdb",
				  flags[i]|MAYBE_NOSYNC,
				  O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ntdb = ntdb_open("run-ntdb_foreach2.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);

		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(found[0] && found[1] && found[2]);

		/* Test premature iteration termination */
		count = 1;
		ntdb_foreach(drop_count, &count);
		ok1(count == 0);

		ntdb_close(ntdb1);
		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(found[0] && !found[1] && found[2]);

		ntdb_close(ntdb);
		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(found[0] && !found[1] && !found[2]);

		ntdb1 = ntdb_open("run-ntdb_foreach1.ntdb",
				  flags[i]|MAYBE_NOSYNC,
				  O_RDWR, 0600, &tap_log_attr);
		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(found[0] && found[1] && !found[2]);

		ntdb_close(ntdb0);
		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(!found[0] && found[1] && !found[2]);

		ntdb_close(ntdb1);
		memset(found, 0, sizeof(found));
		ntdb_foreach(set_found, found);
		ok1(!found[0] && !found[1] && !found[2]);
		ok1(tap_log_messages == 0);
	}

	return exit_status();
}
