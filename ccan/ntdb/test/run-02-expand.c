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
	uint64_t val;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11 + 1);

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		failtest_suppress = true;
		ntdb = ntdb_open("run-expand.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			break;

		val = ntdb->file->map_size;
		/* Need some hash lock for expand. */
		ok1(ntdb_lock_hash(ntdb, 0, F_WRLCK) == 0);
		failtest_suppress = false;
		if (!ok1(ntdb_expand(ntdb, 1) == 0)) {
			failtest_suppress = true;
			ntdb_close(ntdb);
			break;
		}
		failtest_suppress = true;

		ok1(ntdb->file->map_size >= val + 1 * NTDB_EXTENSION_FACTOR);
		ok1(ntdb_unlock_hash(ntdb, 0, F_WRLCK) == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		val = ntdb->file->map_size;
		ok1(ntdb_lock_hash(ntdb, 0, F_WRLCK) == 0);
		failtest_suppress = false;
		if (!ok1(ntdb_expand(ntdb, 1024) == 0)) {
			failtest_suppress = true;
			ntdb_close(ntdb);
			break;
		}
		failtest_suppress = true;
		ok1(ntdb_unlock_hash(ntdb, 0, F_WRLCK) == 0);
		ok1(ntdb->file->map_size >= val + 1024 * NTDB_EXTENSION_FACTOR);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	failtest_exit(exit_status());

	/*
	 * We will never reach this but the compiler complains if we do not
	 * return in this function.
	 */
	return EFAULT;
}
