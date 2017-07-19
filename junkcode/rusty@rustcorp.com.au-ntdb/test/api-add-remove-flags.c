#include "../private.h" // for ntdb_context
#include "../ntdb.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(87);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-add-remove-flags.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_get_flags(ntdb) == ntdb->flags);
		tap_log_messages = 0;
		ntdb_add_flag(ntdb, NTDB_NOLOCK);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(ntdb_get_flags(ntdb) & NTDB_NOLOCK);
		}

		tap_log_messages = 0;
		ntdb_add_flag(ntdb, NTDB_NOMMAP);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(ntdb_get_flags(ntdb) & NTDB_NOMMAP);
			ok1(ntdb->file->map_ptr == NULL);
		}

		tap_log_messages = 0;
		ntdb_add_flag(ntdb, NTDB_NOSYNC);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(ntdb_get_flags(ntdb) & NTDB_NOSYNC);
		}

		ok1(ntdb_get_flags(ntdb) == ntdb->flags);

		tap_log_messages = 0;
		ntdb_remove_flag(ntdb, NTDB_NOLOCK);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(ntdb_get_flags(ntdb) & NTDB_NOLOCK));
		}

		tap_log_messages = 0;
		ntdb_remove_flag(ntdb, NTDB_NOMMAP);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(ntdb_get_flags(ntdb) & NTDB_NOMMAP));
			ok1(ntdb->file->map_ptr != NULL);
		}

		tap_log_messages = 0;
		ntdb_remove_flag(ntdb, NTDB_NOSYNC);
		if (flags[i] & NTDB_INTERNAL)
			ok1(tap_log_messages == 1);
		else {
			ok1(tap_log_messages == 0);
			ok1(!(ntdb_get_flags(ntdb) & NTDB_NOSYNC));
		}

		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
