#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb, *tdb2;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data data = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data d = { NULL, 0 }; /* Bogus GCC warning */
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 28);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-open-multiple-times.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;
		tdb2 = tdb_open("run-open-multiple-times.tdb", flags[i],
				O_RDWR|O_CREAT, 0600, &tap_log_attr);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb_check(tdb2, NULL, NULL) == 0);

		/* Store in one, fetch in the other. */
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
		ok1(tdb_fetch(tdb2, key, &d) == TDB_SUCCESS);
		ok1(tdb_deq(d, data));
		free(d.dptr);

		/* Vice versa, with delete. */
		ok1(tdb_delete(tdb2, key) == 0);
		ok1(tdb_fetch(tdb, key, &d) == TDB_ERR_NOEXIST);

		/* OK, now close first one, check second still good. */
		ok1(tdb_close(tdb) == 0);

		ok1(tdb_store(tdb2, key, data, TDB_REPLACE) == 0);
		ok1(tdb_fetch(tdb2, key, &d) == TDB_SUCCESS);
		ok1(tdb_deq(d, data));
		free(d.dptr);

		/* Reopen */
		tdb = tdb_open("run-open-multiple-times.tdb", flags[i],
			       O_RDWR|O_CREAT, 0600, &tap_log_attr);
		ok1(tdb);

		ok1(tdb_transaction_start(tdb2) == 0);

		/* Anything in the other one should fail. */
		ok1(tdb_fetch(tdb, key, &d) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 2);
		ok1(tdb_transaction_start(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 3);
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 4);

		/* Transaciton should work as normal. */
		ok1(tdb_store(tdb2, key, data, TDB_REPLACE) == TDB_SUCCESS);

		/* Now... try closing with locks held. */
		ok1(tdb_close(tdb2) == 0);

		ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
		ok1(tdb_deq(d, data));
		free(d.dptr);
		ok1(tdb_close(tdb) == 0);
		ok1(tap_log_messages == 4);
		tap_log_messages = 0;
	}

	return exit_status();
}
