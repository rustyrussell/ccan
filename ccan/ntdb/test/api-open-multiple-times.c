#include "config.h"
#include "ntdb.h"
#include "tap-interface.h"
#include <stdlib.h>
#include "logging.h"
#include "../private.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb, *ntdb2;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA d = { NULL, 0 }; /* Bogus GCC warning */
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 30);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-open-multiple-times.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ntdb2 = ntdb_open("run-open-multiple-times.ntdb",
				  flags[i]|MAYBE_NOSYNC,
				  O_RDWR|O_CREAT, 0600, &tap_log_attr);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_check(ntdb2, NULL, NULL) == 0);
		ok1((flags[i] & NTDB_NOMMAP) || ntdb2->file->map_ptr);

		/* Store in one, fetch in the other. */
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
		ok1(ntdb_fetch(ntdb2, key, &d) == NTDB_SUCCESS);
		ok1(ntdb_deq(d, data));
		free(d.dptr);

		/* Vice versa, with delete. */
		ok1(ntdb_delete(ntdb2, key) == 0);
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_ERR_NOEXIST);

		/* OK, now close first one, check second still good. */
		ok1(ntdb_close(ntdb) == 0);

		ok1((flags[i] & NTDB_NOMMAP) || ntdb2->file->map_ptr);
		ok1(ntdb_store(ntdb2, key, data, NTDB_REPLACE) == 0);
		ok1(ntdb_fetch(ntdb2, key, &d) == NTDB_SUCCESS);
		ok1(ntdb_deq(d, data));
		free(d.dptr);

		/* Reopen */
		ntdb = ntdb_open("run-open-multiple-times.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT, 0600, &tap_log_attr);
		ok1(ntdb);

		ok1(ntdb_transaction_start(ntdb2) == 0);

		/* Anything in the other one should fail. */
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 2);
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 3);
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 4);

		/* Transaciton should work as normal. */
		ok1(ntdb_store(ntdb2, key, data, NTDB_REPLACE) == NTDB_SUCCESS);

		/* Now... try closing with locks held. */
		ok1(ntdb_close(ntdb2) == 0);

		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
		ok1(ntdb_deq(d, data));
		free(d.dptr);
		ok1(ntdb_close(ntdb) == 0);
		ok1(tap_log_messages == 4);
		tap_log_messages = 0;
	}

	return exit_status();
}
