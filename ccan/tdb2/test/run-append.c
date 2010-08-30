#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

#define MAX_SIZE 13100
#define SIZE_STEP 131

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	unsigned char *buffer;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = { (unsigned char *)"key", 3 };
	struct tdb_data data;

	buffer = malloc(MAX_SIZE);
	for (i = 0; i < MAX_SIZE; i++)
		buffer[i] = i;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * ((2 + MAX_SIZE/SIZE_STEP * 4) * 2 + 6)
		   + 1);

	/* Using tdb_store. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer;
			data.dsize = j;
			ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			data = tdb_fetch(tdb, key);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
		}
		ok1(!tdb_has_locks(tdb));
		tdb_close(tdb);
	}

	/* Using tdb_append. */
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		size_t prev_len = 0;
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		for (j = 0; j < MAX_SIZE; j += SIZE_STEP) {
			data.dptr = buffer + prev_len;
			data.dsize = j - prev_len;
			ok1(tdb_append(tdb, key, data) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
			data = tdb_fetch(tdb, key);
			ok1(data.dsize == j);
			ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
			free(data.dptr);
			prev_len = data.dsize;
		}
		ok1(!tdb_has_locks(tdb));
		tdb_close(tdb);
	}

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-append.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Huge initial store. */
		data.dptr = buffer;
		data.dsize = MAX_SIZE;
		ok1(tdb_append(tdb, key, data) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		data = tdb_fetch(tdb, key);
		ok1(data.dsize == MAX_SIZE);
		ok1(memcmp(data.dptr, buffer, data.dsize) == 0);
		free(data.dptr);
		ok1(!tdb_has_locks(tdb));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
