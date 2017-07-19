#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 48);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		/* RW -> R0 */
		ntdb = ntdb_open("run-92-get-set-readonly.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		ok1(!(ntdb_get_flags(ntdb) & NTDB_RDONLY));

		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == NTDB_SUCCESS);

		ntdb_add_flag(ntdb, NTDB_RDONLY);
		ok1(ntdb_get_flags(ntdb) & NTDB_RDONLY);

		/* Can't store, append, delete. */
		ok1(ntdb_store(ntdb, key, data, NTDB_MODIFY) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 1);
		ok1(ntdb_append(ntdb, key, data) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 2);
		ok1(ntdb_delete(ntdb, key) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 3);

		/* Can't start a transaction, or any write lock. */
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 4);
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 5);
		ok1(ntdb_lockall(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 6);
		ok1(ntdb_wipe_all(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 7);

		/* Back to RW. */
		ntdb_remove_flag(ntdb, NTDB_RDONLY);
		ok1(!(ntdb_get_flags(ntdb) & NTDB_RDONLY));

		ok1(ntdb_store(ntdb, key, data, NTDB_MODIFY) == NTDB_SUCCESS);
		ok1(ntdb_append(ntdb, key, data) == NTDB_SUCCESS);
		ok1(ntdb_delete(ntdb, key) == NTDB_SUCCESS);

		ok1(ntdb_transaction_start(ntdb) == NTDB_SUCCESS);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == NTDB_SUCCESS);
		ok1(ntdb_transaction_commit(ntdb) == NTDB_SUCCESS);

		ok1(ntdb_chainlock(ntdb, key) == NTDB_SUCCESS);
		ntdb_chainunlock(ntdb, key);
		ok1(ntdb_lockall(ntdb) == NTDB_SUCCESS);
		ntdb_unlockall(ntdb);
		ok1(ntdb_wipe_all(ntdb) == NTDB_SUCCESS);
		ok1(tap_log_messages == 7);

		ntdb_close(ntdb);

		/* R0 -> RW */
		ntdb = ntdb_open("run-92-get-set-readonly.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDONLY, 0600, &tap_log_attr);
		ok1(ntdb);
		ok1(ntdb_get_flags(ntdb) & NTDB_RDONLY);

		/* Can't store, append, delete. */
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 8);
		ok1(ntdb_append(ntdb, key, data) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 9);
		ok1(ntdb_delete(ntdb, key) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 10);

		/* Can't start a transaction, or any write lock. */
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 11);
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 12);
		ok1(ntdb_lockall(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 13);
		ok1(ntdb_wipe_all(ntdb) == NTDB_ERR_RDONLY);
		ok1(tap_log_messages == 14);

		/* Can't remove NTDB_RDONLY since we opened with O_RDONLY */
		ntdb_remove_flag(ntdb, NTDB_RDONLY);
		ok1(tap_log_messages == 15);
		ok1(ntdb_get_flags(ntdb) & NTDB_RDONLY);
		ntdb_close(ntdb);

		ok1(tap_log_messages == 15);
		tap_log_messages = 0;
	}
	return exit_status();
}
