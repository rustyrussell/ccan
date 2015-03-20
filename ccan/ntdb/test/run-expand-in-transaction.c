#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "helprun-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 9 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t size;
		NTDB_DATA k, d;
		ntdb = ntdb_open("run-expand-in-transaction.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		size = ntdb->file->map_size;
		/* Add a fake record to chew up the existing free space. */
		k = ntdb_mkdata("fake", 4);
		d.dsize = ntdb->file->map_size
			- NEW_DATABASE_HDR_SIZE(ntdb->hash_bits) - 8;
		d.dptr = malloc(d.dsize);
		memset(d.dptr, 0, d.dsize);
		ok1(ntdb_store(ntdb, k, d, NTDB_INSERT) == 0);
		ok1(ntdb->file->map_size == size);
		free(d.dptr);
		ok1(ntdb_transaction_start(ntdb) == 0);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb->file->map_size > size);
		ok1(ntdb_transaction_commit(ntdb) == 0);
		ok1(ntdb->file->map_size > size);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
