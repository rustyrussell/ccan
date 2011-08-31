#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i, extra_msgs;
	struct tdb_context *tdb;
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
			TDB_VERSION1, TDB_NOMMAP|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 48);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		/* RW -> R0 */
		tdb = tdb_open("run-92-get-set-readonly.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		ok1(!(tdb_get_flags(tdb) & TDB_RDONLY));

		/* TDB1 complains multiple times. */
		if (flags[i] & TDB_VERSION1) {
			extra_msgs = 1;
		} else {
			extra_msgs = 0;
		}

		ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);

		tdb_add_flag(tdb, TDB_RDONLY);
		ok1(tdb_get_flags(tdb) & TDB_RDONLY);

		/* Can't store, append, delete. */
		ok1(tdb_store(tdb, key, data, TDB_MODIFY) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 1);
		ok1(tdb_append(tdb, key, data) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 2);
		ok1(tdb_delete(tdb, key) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 3);

		/* Can't start a transaction, or any write lock. */
		ok1(tdb_transaction_start(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 4);
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 5);
		ok1(tdb_lockall(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 6);
		ok1(tdb_wipe_all(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 7);

		/* Back to RW. */
		tdb_remove_flag(tdb, TDB_RDONLY);
		ok1(!(tdb_get_flags(tdb) & TDB_RDONLY));

		ok1(tdb_store(tdb, key, data, TDB_MODIFY) == TDB_SUCCESS);
		ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
		ok1(tdb_delete(tdb, key) == TDB_SUCCESS);

		ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
		ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);

		ok1(tdb_chainlock(tdb, key) == TDB_SUCCESS);
		tdb_chainunlock(tdb, key);
		ok1(tdb_lockall(tdb) == TDB_SUCCESS);
		tdb_unlockall(tdb);
		ok1(tdb_wipe_all(tdb) == TDB_SUCCESS);
		ok1(tap_log_messages == 7);

		tdb_close(tdb);

		/* R0 -> RW */
		tdb = tdb_open("run-92-get-set-readonly.tdb", flags[i],
			       O_RDONLY, 0600, &tap_log_attr);
		ok1(tdb);
		ok1(tdb_get_flags(tdb) & TDB_RDONLY);

		/* Can't store, append, delete. */
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 8);
		ok1(tdb_append(tdb, key, data) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 9);
		ok1(tdb_delete(tdb, key) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 10);

		/* Can't start a transaction, or any write lock. */
		ok1(tdb_transaction_start(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 11);
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_RDONLY);
		tap_log_messages -= extra_msgs;
		ok1(tap_log_messages == 12);
		ok1(tdb_lockall(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 13);
		ok1(tdb_wipe_all(tdb) == TDB_ERR_RDONLY);
		ok1(tap_log_messages == 14);

		/* Can't remove TDB_RDONLY since we opened with O_RDONLY */
		tdb_remove_flag(tdb, TDB_RDONLY);
		ok1(tap_log_messages == 15);
		ok1(tdb_get_flags(tdb) & TDB_RDONLY);
		tdb_close(tdb);

		ok1(tap_log_messages == 15);
		tap_log_messages = 0;
	}
	return exit_status();
}
