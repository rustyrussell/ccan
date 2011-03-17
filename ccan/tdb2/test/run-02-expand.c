#include <ccan/failtest/failtest_override.h>
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>
#include "logging.h"
#include "failtest_helper.h"

static bool failtest_suppress = false;

/* Don't need to test everything here, just want expand testing. */
static enum failtest_result
suppress_failure(struct failtest_call *history, unsigned num)
{
	if (failtest_suppress)
		return FAIL_DONT_FAIL;
	return block_repeat_failures(history, num);
}

int main(int argc, char *argv[])
{
	unsigned int i;
	uint64_t val;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11 + 1);

	failtest_init(argc, argv);
	failtest_hook = suppress_failure;
	failtest_exit_check = exit_check_log;

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		failtest_suppress = true;
		tdb = tdb_open("run-expand.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			break;

		val = tdb->map_size;
		/* Need some hash lock for expand. */
		ok1(tdb_lock_hashes(tdb, 0, 1, F_WRLCK, TDB_LOCK_WAIT) == 0);
		failtest_suppress = false;
		if (!ok1(tdb_expand(tdb, 1) == 0)) {
			failtest_suppress = true;
			tdb_close(tdb);
			break;
		}
		failtest_suppress = true;
			
		ok1(tdb->map_size >= val + 1 * TDB_EXTENSION_FACTOR);
		ok1(tdb_unlock_hashes(tdb, 0, 1, F_WRLCK) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		val = tdb->map_size;
		ok1(tdb_lock_hashes(tdb, 0, 1, F_WRLCK, TDB_LOCK_WAIT) == 0);
		failtest_suppress = false;
		if (!ok1(tdb_expand(tdb, 1024) == 0)) {
			failtest_suppress = true;
			tdb_close(tdb);
			break;
		}
		failtest_suppress = true;
		ok1(tdb_unlock_hashes(tdb, 0, 1, F_WRLCK) == 0);
		ok1(tdb->map_size >= val + 1024 * TDB_EXTENSION_FACTOR);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	failtest_exit(exit_status());
}
