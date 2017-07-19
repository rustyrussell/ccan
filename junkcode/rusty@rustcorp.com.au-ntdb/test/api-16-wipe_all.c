#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

static bool add_records(struct ntdb_context *ntdb)
{
	int i;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < 1000; i++) {
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
			return false;
	}
	return true;
}


int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-16-wipe_all.ntdb", flags[i]|MAYBE_NOSYNC,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (ok1(ntdb)) {
			NTDB_DATA key;
			ok1(add_records(ntdb));
			ok1(ntdb_wipe_all(ntdb) == NTDB_SUCCESS);
			ok1(ntdb_firstkey(ntdb, &key) == NTDB_ERR_NOEXIST);
			ntdb_close(ntdb);
		}
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
