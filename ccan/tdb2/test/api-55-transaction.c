#include <ccan/tdb2/private.h> // struct tdb_context
#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	unsigned char *buffer;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
			TDB_VERSION1, TDB_NOMMAP|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data;

	buffer = malloc(1000);
	for (i = 0; i < 1000; i++)
		buffer[i] = i;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 20 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-55-transaction.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_transaction_start(tdb) == 0);
		data.dptr = buffer;
		data.dsize = 1000;
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);

		/* Cancelling a transaction means no store */
		tdb_transaction_cancel(tdb);
		ok1(tdb->file->allrecord_lock.count == 0
		    && tdb->file->num_lockrecs == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb_fetch(tdb, key, &data) == TDB_ERR_NOEXIST);

		/* Commit the transaction. */
		ok1(tdb_transaction_start(tdb) == 0);
		data.dptr = buffer;
		data.dsize = 1000;
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);
		ok1(tdb_transaction_commit(tdb) == 0);
		ok1(tdb->file->allrecord_lock.count == 0
		    && tdb->file->num_lockrecs == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb_fetch(tdb, key, &data) == TDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	free(buffer);
	return exit_status();
}
