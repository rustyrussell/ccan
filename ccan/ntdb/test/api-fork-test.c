/* Test forking while holding lock.
 *
 * There are only five ways to do this currently:
 * (1) grab a ntdb_chainlock, then fork.
 * (2) grab a ntdb_lockall, then fork.
 * (3) grab a ntdb_lockall_read, then fork.
 * (4) start a transaction, then fork.
 * (5) fork from inside a ntdb_parse() callback.
 *
 * Note that we don't hold a lock across ntdb_traverse callbacks, so
 * that doesn't matter.
 */
#include "config.h"
#include "ntdb.h"
#include "private.h"
#include "tap-interface.h"
#include "logging.h"

static bool am_child = false;

static enum NTDB_ERROR fork_in_parse(NTDB_DATA key, NTDB_DATA data,
				    struct ntdb_context *ntdb)
{
	int status;

	if (fork() == 0) {
		am_child = true;

		/* We expect this to fail. */
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != NTDB_ERR_LOCK)
			exit(1);

		if (ntdb_fetch(ntdb, key, &data) != NTDB_ERR_LOCK)
			exit(1);

		if (tap_log_messages != 2)
			exit(2);

		return NTDB_SUCCESS;
	}
	wait(&status);
	ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return NTDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 14);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		int status;

		tap_log_messages = 0;

		ntdb = ntdb_open("run-fork-test.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			continue;

		/* Put a record in here. */
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == NTDB_SUCCESS);

		ok1(ntdb_chainlock(ntdb, key) == NTDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != NTDB_ERR_LOCK)
				return 1;

			if (ntdb_fetch(ntdb, key, &data) != NTDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			/* Child can do this without any complaints. */
			ntdb_chainunlock(ntdb, key);
			if (tap_log_messages != 2)
				return 3;
			ntdb_close(ntdb);
			if (tap_log_messages != 2)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		ntdb_chainunlock(ntdb, key);

		ok1(ntdb_lockall(ntdb) == NTDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != NTDB_ERR_LOCK)
				return 1;

			if (ntdb_fetch(ntdb, key, &data) != NTDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			/* Child can do this without any complaints. */
			ntdb_unlockall(ntdb);
			if (tap_log_messages != 2)
				return 3;
			ntdb_close(ntdb);
			if (tap_log_messages != 2)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		ntdb_unlockall(ntdb);

		ok1(ntdb_lockall_read(ntdb) == NTDB_SUCCESS);
		if (fork() == 0) {
			/* We expect this to fail. */
			/* This would always fail anyway... */
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != NTDB_ERR_LOCK)
				return 1;

			if (ntdb_fetch(ntdb, key, &data) != NTDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			/* Child can do this without any complaints. */
			ntdb_unlockall_read(ntdb);
			if (tap_log_messages != 2)
				return 3;
			ntdb_close(ntdb);
			if (tap_log_messages != 2)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		ntdb_unlockall_read(ntdb);

		ok1(ntdb_transaction_start(ntdb) == NTDB_SUCCESS);
		/* If transactions is empty, noop "commit" succeeds. */
		ok1(ntdb_delete(ntdb, key) == NTDB_SUCCESS);
		if (fork() == 0) {
			int last_log_messages;

			/* We expect this to fail. */
			if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != NTDB_ERR_LOCK)
				return 1;

			if (ntdb_fetch(ntdb, key, &data) != NTDB_ERR_LOCK)
				return 1;

			if (tap_log_messages != 2)
				return 2;

			if (ntdb_transaction_prepare_commit(ntdb)
			    != NTDB_ERR_LOCK)
				return 3;
			if (tap_log_messages == 2)
				return 4;

			last_log_messages = tap_log_messages;
			/* Child can do this without any complaints. */
			ntdb_transaction_cancel(ntdb);
			if (tap_log_messages != last_log_messages)
				return 4;
			ntdb_close(ntdb);
			if (tap_log_messages != last_log_messages)
				return 4;
			return 0;
		}
		wait(&status);
		ok1(WIFEXITED(status) && WEXITSTATUS(status) == 0);
		ntdb_transaction_cancel(ntdb);

		ok1(ntdb_parse_record(ntdb, key, fork_in_parse, ntdb)
		    == NTDB_SUCCESS);
		ntdb_close(ntdb);
		if (am_child) {
			/* Child can return from parse without complaints. */
			if (tap_log_messages != 2)
				exit(3);
			exit(0);
		}
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
