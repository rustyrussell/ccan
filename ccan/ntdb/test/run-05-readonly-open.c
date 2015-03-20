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
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4), d;
	union ntdb_attribute seed_attr;
	unsigned int msgs = 0;

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check_log;

	seed_attr.base.attr = NTDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &tap_log_attr;
	seed_attr.seed.seed = 0;

	failtest_suppress = true;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-05-readonly-open.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600,
				 &seed_attr);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ntdb_close(ntdb);

		failtest_suppress = false;
		ntdb = ntdb_open("run-05-readonly-open.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDONLY, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			break;
		ok1(tap_log_messages == msgs);
		/* Fetch should succeed, stores should fail. */
		if (!ok1(ntdb_fetch(ntdb, key, &d) == 0))
			goto fail;
		ok1(ntdb_deq(d, data));
		free(d.dptr);
		if (!ok1(ntdb_store(ntdb, key, data, NTDB_MODIFY)
			 == NTDB_ERR_RDONLY))
			goto fail;
		ok1(tap_log_messages == ++msgs);
		if (!ok1(ntdb_store(ntdb, key, data, NTDB_INSERT)
			 == NTDB_ERR_RDONLY))
			goto fail;
		ok1(tap_log_messages == ++msgs);
		failtest_suppress = true;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ntdb_close(ntdb);
		ok1(tap_log_messages == msgs);
		/* SIGH: failtest bug, it doesn't save the ntdb file because
		 * we have it read-only.  If we go around again, it gets
		 * changed underneath us and things get screwy. */
		if (failtest_has_failed())
			break;
	}
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
