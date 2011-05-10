/* Test forking while holding lock.
 *
 * There are only five ways to do this currently:
 * (1) grab a tdb_chainlock, then fork.
 * (2) grab a tdb_lockall, then fork.
 * (3) grab a tdb_lockall_read, then fork.
 * (4) start a transaction, then fork.
 * (5) fork from inside a tdb_parse() callback.
 *
 * Note that we don't hold a lock across tdb_traverse callbacks, so
 * that doesn't matter.
 */
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "logging.h"

static enum TDB_ERROR fork_in_parse(TDB_DATA key, TDB_DATA data,
				    struct tdb_context *tdb)
{
	int status;

	if (fork() == 0) {
		/* We expect this to fail. */
		if (tdb_store(tdb, key, data, TDB_REPLACE) != TDB_ERR_LOCK)
			exit(1);

		if (tdb_fetch(tdb, key, &data) != TDB_ERR_LOCK)
			exit(1);

		if (tap_log_messages != 2)
			exit(2);

		tdb_close(tdb);
		if (tap_log_messages != 2)
			exit(3);
		exit(0);
	}
	wait(&status);
	ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return TDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 14);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		int status;

		tap_log_messages = 0;

		tdb = tdb_open("run-fork-test.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			continue;

		/* Put a record in here. */
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == TDB_SUCCESS);

		ok1(tdb_chainlock(tdb, key) == TDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			if (tdb_store(tdb, key, data, TDB_REPLACE) != TDB_ERR_LOCK)
				return 1;

			if (tdb_fetch(tdb, key, &data) != TDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			tdb_chainunlock(tdb, key);
			if (tap_log_messages != 3)
				return 3;
			tdb_close(tdb);
			if (tap_log_messages != 3)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		tdb_chainunlock(tdb, key);

		ok1(tdb_lockall(tdb) == TDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			if (tdb_store(tdb, key, data, TDB_REPLACE) != TDB_ERR_LOCK)
				return 1;

			if (tdb_fetch(tdb, key, &data) != TDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			tdb_unlockall(tdb);
			if (tap_log_messages != 2)
				return 3;
			tdb_close(tdb);
			if (tap_log_messages != 2)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		tdb_unlockall(tdb);

		ok1(tdb_lockall_read(tdb) == TDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			/* This would always fail anyway... */
			if (tdb_store(tdb, key, data, TDB_REPLACE) != TDB_ERR_LOCK)
				return 1;

			if (tdb_fetch(tdb, key, &data) != TDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			tdb_unlockall_read(tdb);
			if (tap_log_messages != 2)
				return 3;
			tdb_close(tdb);
			if (tap_log_messages != 2)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		tdb_unlockall_read(tdb);

		ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
		/* If transactions is empty, noop "commit" succeeds. */
		ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			if (tdb_store(tdb, key, data, TDB_REPLACE) != TDB_ERR_LOCK)
				return 1;

			if (tdb_fetch(tdb, key, &data) != TDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			if (tdb_transaction_commit(tdb) != TDB_ERR_LOCK)
				return 3;

			tdb_close(tdb);
			if (tap_log_messages < 3)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		tdb_transaction_cancel(tdb);

		ok1(tdb_parse_record(tdb, key, fork_in_parse, tdb)
		    == TDB_SUCCESS);
		tdb_close(tdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
