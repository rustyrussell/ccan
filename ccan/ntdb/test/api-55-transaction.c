#include "private.h" // struct ntdb_context
#include "ntdb.h"
#include "tap-interface.h"
#include <stdlib.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	unsigned char *buffer;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data;

	buffer = malloc(1000);
	for (i = 0; i < 1000; i++)
		buffer[i] = i;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 20 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-55-transaction.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_transaction_start(ntdb) == 0);
		data.dptr = buffer;
		data.dsize = 1000;
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);

		/* Cancelling a transaction means no store */
		ntdb_transaction_cancel(ntdb);
		ok1(ntdb->file->allrecord_lock.count == 0
		    && ntdb->file->num_lockrecs == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_fetch(ntdb, key, &data) == NTDB_ERR_NOEXIST);

		/* Commit the transaction. */
		ok1(ntdb_transaction_start(ntdb) == 0);
		data.dptr = buffer;
		data.dsize = 1000;
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);
		ok1(ntdb_transaction_commit(ntdb) == 0);
		ok1(ntdb->file->allrecord_lock.count == 0
		    && ntdb->file->num_lockrecs == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_fetch(ntdb, key, &data) == NTDB_SUCCESS);
		ok1(data.dsize == 1000);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);

		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	free(buffer);
	return exit_status();
}
