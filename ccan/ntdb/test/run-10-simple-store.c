#include <ccan/failtest/failtest_override.h>
#include "ntdb-source.h"
#include "tap-interface.h"
#include <ccan/failtest/failtest.h>
#include "logging.h"
#include "failtest_helper.h"
#include "helprun-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;

	failtest_suppress = true;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-10-simple-store.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			break;
		/* Modify should fail. */
		failtest_suppress = false;
		if (!ok1(ntdb_store(ntdb, key, data, NTDB_MODIFY)
			 == NTDB_ERR_NOEXIST))
			goto fail;
		failtest_suppress = true;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		/* Insert should succeed. */
		failtest_suppress = false;
		if (!ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0))
			goto fail;
		failtest_suppress = true;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		/* Second insert should fail. */
		failtest_suppress = false;
		if (!ok1(ntdb_store(ntdb, key, data, NTDB_INSERT)
			 == NTDB_ERR_EXISTS))
			goto fail;
		failtest_suppress = true;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ntdb_close(ntdb);
	}
	ok1(tap_log_messages == 0);
	failtest_exit(exit_status());

fail:
	failtest_suppress = true;
	ntdb_close(ntdb);
	failtest_exit(exit_status());

	/*
	 * We will never reach this but the compiler complains if we do not
	 * return in this function.
	 */
	return EFAULT;
}
