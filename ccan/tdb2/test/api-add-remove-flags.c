#include <ccan/tdb2/private.h> // for tdb_context
#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	plan_tests(173);
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
			ok1(tdb->file->map_ptr == NULL);
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
			ok1(tdb->file->map_ptr != NULL);
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
