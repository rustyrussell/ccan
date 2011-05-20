#include <ccan/failtest/failtest_override.h>
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
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
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4), d;
	union tdb_attribute seed_attr;
	unsigned int msgs = 0;

	failtest_init(argc, argv);
	failtest_hook = suppress_failure;
	failtest_exit_check = exit_check_log;

	seed_attr.base.attr = TDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &tap_log_attr;
	seed_attr.seed.seed = 0;

	failtest_suppress = true;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 11);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-05-readonly-open.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &seed_attr);
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		tdb_close(tdb);

		failtest_suppress = false;
		tdb = tdb_open("run-05-readonly-open.tdb", flags[i],
			       O_RDONLY, 0600, &tap_log_attr);
		if (!ok1(tdb))
			break;
		ok1(tap_log_messages == msgs);
		/* Fetch should succeed, stores should fail. */
		if (!ok1(tdb_fetch(tdb, key, &d) == 0))
			goto fail;
		ok1(tdb_deq(d, data));
		free(d.dptr);
		if (!ok1(tdb_store(tdb, key, data, TDB_MODIFY)
			 == TDB_ERR_RDONLY))
			goto fail;
		ok1(tap_log_messages == ++msgs);
		if (!ok1(tdb_store(tdb, key, data, TDB_INSERT)
			 == TDB_ERR_RDONLY))
			goto fail;
		ok1(tap_log_messages == ++msgs);
		failtest_suppress = true;
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
		ok1(tap_log_messages == msgs);
		/* SIGH: failtest bug, it doesn't save the tdb file because
		 * we have it read-only.  If we go around again, it gets
		 * changed underneath us and things get screwy. */
		if (failtest_has_failed())
			break;
	}
	failtest_exit(exit_status());

fail:
	failtest_suppress = true;
	tdb_close(tdb);
	failtest_exit(exit_status());
}
