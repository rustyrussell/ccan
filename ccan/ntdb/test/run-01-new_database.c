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

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-new_database.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			failtest_exit(exit_status());

		failtest_suppress = true;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		failtest_suppress = false;
		ntdb_close(ntdb);
		if (!ok1(tap_log_messages == 0))
			break;
	}
	failtest_exit(exit_status());

	/*
	 * We will never reach this but the compiler complains if we do not
	 * return in this function.
	 */
	return EFAULT;
}
