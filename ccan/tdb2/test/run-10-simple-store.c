#include <ccan/failtest/failtest_override.h>
#include "tdb2-source.h"
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
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;

	failtest_suppress = true;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-10-simple-store.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			break;
		/* Modify should fail. */
		failtest_suppress = false;
		if (!ok1(tdb_store(tdb, key, data, TDB_MODIFY)
			 == TDB_ERR_NOEXIST))
			goto fail;
		failtest_suppress = true;
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		/* Insert should succeed. */
		failtest_suppress = false;
		if (!ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0))
			goto fail;
		failtest_suppress = true;
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		/* Second insert should fail. */
		failtest_suppress = false;
		if (!ok1(tdb_store(tdb, key, data, TDB_INSERT)
			 == TDB_ERR_EXISTS))
			goto fail;
		failtest_suppress = true;
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}
	ok1(tap_log_messages == 0);
	failtest_exit(exit_status());

fail:
	failtest_suppress = true;
	tdb_close(tdb);
	failtest_exit(exit_status());
}
