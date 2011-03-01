#include <ccan/failtest/failtest_override.h>
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>
#include "logging.h"
#include "failtest_helper.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-new_database.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			failtest_exit(exit_status());
		if (tdb) {
			bool ok = ok1(tdb_check(tdb, NULL, NULL) == 0);
			tdb_close(tdb);
			if (!ok)
				failtest_exit(exit_status());
		}
		if (!ok1(tap_log_messages == 0))
			break;
	}
	failtest_exit(exit_status());
}
