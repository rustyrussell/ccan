#include "config.h"
#include "ntdb.h"
#include "private.h"
#include "tap-interface.h"
#include "logging.h"

#define MAX_SIZE 10000
#define SIZE_STEP 131

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data;

	data.dptr = malloc(MAX_SIZE);
	memset(data.dptr, 0x24, MAX_SIZE);

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (3 + (1 + (MAX_SIZE/SIZE_STEP)) * 2) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-record-expand.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		data.dsize = 0;
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		for (data.dsize = 0;
		     data.dsize < MAX_SIZE;
		     data.dsize += SIZE_STEP) {
			memset(data.dptr, data.dsize, data.dsize);
			ok1(ntdb_store(ntdb, key, data, NTDB_MODIFY) == 0);
			ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		}
		ntdb_close(ntdb);
	}
	ok1(tap_log_messages == 0);
	free(data.dptr);

	return exit_status();
}
