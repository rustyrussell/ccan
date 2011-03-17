#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(87);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-add-remove-flags.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_get_flags(tdb) == tdb->flags);
		tap_log_messages = 0;
		tdb_add_flag(tdb, TDB_NOLOCK);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(tdb_get_flags(tdb) & TDB_NOLOCK);
		}

		tap_log_messages = 0;
		tdb_add_flag(tdb, TDB_NOMMAP);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(tdb_get_flags(tdb) & TDB_NOMMAP);
			ok1(tdb->map_ptr == NULL);
		}

		tap_log_messages = 0;
		tdb_add_flag(tdb, TDB_NOSYNC);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(tdb_get_flags(tdb) & TDB_NOSYNC);
		}

		ok1(tdb_get_flags(tdb) == tdb->flags);

		tap_log_messages = 0;
		tdb_remove_flag(tdb, TDB_NOLOCK);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(tdb_get_flags(tdb) & TDB_NOLOCK));
		}

		tap_log_messages = 0;
		tdb_remove_flag(tdb, TDB_NOMMAP);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(tdb_get_flags(tdb) & TDB_NOMMAP));
			ok1(tdb->map_ptr != NULL);
		}

		tap_log_messages = 0;
		tdb_remove_flag(tdb, TDB_NOSYNC);
		if (flags[i] & TDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(tdb_get_flags(tdb) & TDB_NOSYNC));
		}

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
