#include <ccan/tdb2/private.h>
#include <ccan/failtest/failtest_override.h>
#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>
#include "logging.h"
#include "failtest_helper.h"

int main(int argc, char *argv[])
{
	unsigned int i, messages = 0;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-35-convert.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			failtest_exit(exit_status());

		tdb_close(tdb);
		/* If we say TDB_CONVERT, it must be converted */
		tdb = tdb_open("run-35-convert.tdb",
			       flags[i]|TDB_CONVERT,
			       O_RDWR, 0600, &tap_log_attr);
		if (flags[i] & TDB_CONVERT) {
			if (!tdb)
				failtest_exit(exit_status());
			ok1(tdb_get_flags(tdb) & TDB_CONVERT);
			tdb_close(tdb);
		} else {
			if (!ok1(!tdb && errno == EIO))
				failtest_exit(exit_status());
			ok1(tap_log_messages == ++messages);
			if (!ok1(log_last && strstr(log_last, "TDB_CONVERT")))
				failtest_exit(exit_status());
		}

		/* If don't say TDB_CONVERT, it *may* be converted */
		tdb = tdb_open("run-35-convert.tdb",
			       flags[i] & ~TDB_CONVERT,
			       O_RDWR, 0600, &tap_log_attr);
		if (!tdb)
			failtest_exit(exit_status());
		ok1(tdb_get_flags(tdb) == flags[i]);
		tdb_close(tdb);
	}
	failtest_exit(exit_status());
}
