#include "../private.h"
#include <ccan/failtest/failtest_override.h>
#include "ntdb-source.h"
#include "tap-interface.h"
#include <ccan/failtest/failtest.h>
#include "logging.h"
#include "failtest_helper.h"
#include "helprun-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i, messages = 0;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-35-convert.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			failtest_exit(exit_status());

		ntdb_close(ntdb);
		/* We can fail in log message formatting or open.  That's OK */
		if (failtest_has_failed()) {
			failtest_exit(exit_status());
		}
		/* If we say NTDB_CONVERT, it must be converted */
		ntdb = ntdb_open("run-35-convert.ntdb",
				 flags[i]|NTDB_CONVERT|MAYBE_NOSYNC,
				 O_RDWR, 0600, &tap_log_attr);
		if (flags[i] & NTDB_CONVERT) {
			if (!ntdb)
				failtest_exit(exit_status());
			ok1(ntdb_get_flags(ntdb) & NTDB_CONVERT);
			ntdb_close(ntdb);
		} else {
			if (!ok1(!ntdb && errno == EIO))
				failtest_exit(exit_status());
			ok1(tap_log_messages == ++messages);
			if (!ok1(log_last && strstr(log_last, "NTDB_CONVERT")))
				failtest_exit(exit_status());
		}

		/* If don't say NTDB_CONVERT, it *may* be converted */
		ntdb = ntdb_open("run-35-convert.ntdb",
				 (flags[i] & ~NTDB_CONVERT)|MAYBE_NOSYNC,
				 O_RDWR, 0600, &tap_log_attr);
		if (!ntdb)
			failtest_exit(exit_status());
		ok1(ntdb_get_flags(ntdb) == (flags[i]|MAYBE_NOSYNC));
		ntdb_close(ntdb);
	}
	failtest_exit(exit_status());

	/*
	 * We will never reach this but the compiler complains if we do not
	 * return in this function.
	 */
	return EFAULT;
}
