#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include "logging.h"

static int drop_count(struct tdb_context *tdb, unsigned int *count)
{
	if (--(*count) == 0)
		return 1;
	return 0;
}

static int set_found(struct tdb_context *tdb, bool found[3])
{
	unsigned int idx;

	if (strcmp(tdb_name(tdb), "run-tdb_foreach0.tdb") == 0)
		idx = 0;
	else if (strcmp(tdb_name(tdb), "run-tdb_foreach1.tdb") == 0)
		idx = 1;
	else if (strcmp(tdb_name(tdb), "run-tdb_foreach2.tdb") == 0)
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
	struct tdb_context *tdb0, *tdb1, *tdb2;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 8);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb0 = tdb_open("run-tdb_foreach0.tdb", flags[i],
				O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		tdb1 = tdb_open("run-tdb_foreach1.tdb", flags[i],
				O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		tdb2 = tdb_open("run-tdb_foreach2.tdb", flags[i],
				O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);

		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(found[0] && found[1] && found[2]);

		/* Test premature iteration termination */
		count = 1;
		tdb_foreach(drop_count, &count);
		ok1(count == 0);

		tdb_close(tdb1);
		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(found[0] && !found[1] && found[2]);

		tdb_close(tdb2);
		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(found[0] && !found[1] && !found[2]);

		tdb1 = tdb_open("run-tdb_foreach1.tdb", flags[i],
				O_RDWR, 0600, &tap_log_attr);
		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(found[0] && found[1] && !found[2]);

		tdb_close(tdb0);
		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(!found[0] && found[1] && !found[2]);

		tdb_close(tdb1);
		memset(found, 0, sizeof(found));
		tdb_foreach(set_found, found);
		ok1(!found[0] && !found[1] && !found[2]);
		ok1(tap_log_messages == 0);
	}

	return exit_status();
}
