#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	struct tdb_data d = { NULL, 0 }; /* Bogus GCC warning */
	struct tdb_data key = { (unsigned char *)"key", 3 };
	struct tdb_data data = { (unsigned char *)"data", 4 };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 15 + 4 * 13);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-new_database.tdb", flags[i]|TDB_SEQNUM,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			continue;
		
		ok1(tdb_get_seqnum(tdb) == 0);
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_get_seqnum(tdb) == 1);
		/* Fetch doesn't change seqnum */
		if (ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS))
			free(d.dptr);
		ok1(tdb_get_seqnum(tdb) == 1);
		ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == 2);

		ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == 3);
		/* Empty append works */
		ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == 4);

		ok1(tdb_wipe_all(tdb) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == 5);

		if (!(flags[i] & TDB_INTERNAL)) {
			ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
			ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
			ok1(tdb_get_seqnum(tdb) == 6);
			ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == 7);
			ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == 8);
			ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == 8);

			ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
			ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
			ok1(tdb_get_seqnum(tdb) == 9);
			tdb_transaction_cancel(tdb);
			ok1(tdb_get_seqnum(tdb) == 8);
		}
		tdb_close(tdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
