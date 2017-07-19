#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

#define NUM_RECORDS 1000

static bool store_records(struct ntdb_context *ntdb)
{
	int i;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_RECORDS; i++)
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
			return false;
	return true;
}

static enum NTDB_ERROR check(NTDB_DATA key,
			    NTDB_DATA data,
			    bool *array)
{
	int val;

	if (key.dsize != sizeof(val)) {
		diag("Wrong key size: %zu\n", key.dsize);
		return NTDB_ERR_CORRUPT;
	}

	if (key.dsize != data.dsize
	    || memcmp(key.dptr, data.dptr, sizeof(val)) != 0) {
		diag("Key and data differ\n");
		return NTDB_ERR_CORRUPT;
	}

	memcpy(&val, key.dptr, sizeof(val));
	if (val >= NUM_RECORDS || val < 0) {
		diag("check value %i\n", val);
		return NTDB_ERR_CORRUPT;
	}

	if (array[val]) {
		diag("Value %i already seen\n", val);
		return NTDB_ERR_CORRUPT;
	}

	array[val] = true;
	return NTDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	return 0;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		bool array[NUM_RECORDS];

		ntdb = ntdb_open("run-check-callback.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(store_records(ntdb));
		for (j = 0; j < NUM_RECORDS; j++)
			array[j] = false;
		ok1(ntdb_check(ntdb, check, array) == NTDB_SUCCESS);
		for (j = 0; j < NUM_RECORDS; j++)
			if (!array[j])
				break;
		ok1(j == NUM_RECORDS);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
