#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logging.h"

#define NUM_RECORDS 1000

static bool store_records(struct tdb_context *tdb)
{
	int i;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_RECORDS; i++)
		if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
			return false;
	return true;
}

static enum TDB_ERROR check(struct tdb_data key,
			    struct tdb_data data,
			    bool *array)
{
	int val;

	if (key.dsize != sizeof(val)) {
		diag("Wrong key size: %u\n", key.dsize);
		return TDB_ERR_CORRUPT;
	}

	if (key.dsize != data.dsize
	    || memcmp(key.dptr, data.dptr, sizeof(val)) != 0) {
		diag("Key and data differ\n");
		return TDB_ERR_CORRUPT;
	}

	memcpy(&val, key.dptr, sizeof(val));
	if (val >= NUM_RECORDS || val < 0) {
		diag("check value %i\n", val);
		return TDB_ERR_CORRUPT;
	}

	if (array[val]) {
		diag("Value %i already seen\n", val);
		return TDB_ERR_CORRUPT;
	}

	array[val] = true;
	return TDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		bool array[NUM_RECORDS];

		tdb = tdb_open("run-check-callback.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(store_records(tdb));
		for (j = 0; j < NUM_RECORDS; j++)
			array[j] = false;
		ok1(tdb_check(tdb, check, array) == TDB_SUCCESS);
		for (j = 0; j < NUM_RECORDS; j++)
			if (!array[j])
				break;
		ok1(j == NUM_RECORDS);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
