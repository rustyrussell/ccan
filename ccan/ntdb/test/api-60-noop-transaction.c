#include "private.h" // struct ntdb_context
#include "ntdb.h"
#include "tap-interface.h"
#include <stdlib.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4), d;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 12 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-60-transaction.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);

		ok1(ntdb_transaction_start(ntdb) == 0);
		/* Do an identical replace. */
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
		ok1(ntdb_transaction_commit(ntdb) == 0);

		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
		ok1(ntdb_deq(data, d));
		free(d.dptr);
		ntdb_close(ntdb);

		/* Reopen, fetch. */
		ntdb = ntdb_open("api-60-transaction.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
		ok1(ntdb_deq(data, d));
		free(d.dptr);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
