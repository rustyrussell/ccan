#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

#define NUM_TESTS 1000

static bool store_all(struct ntdb_context *ntdb)
{
	unsigned int i;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA dbuf = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_TESTS; i++) {
		if (ntdb_store(ntdb, key, dbuf, NTDB_INSERT) != NTDB_SUCCESS)
			return false;
	}
	return true;
}

static int mark_entry(struct ntdb_context *ntdb,
		      NTDB_DATA key, NTDB_DATA data, bool found[])
{
	unsigned int num;

	if (key.dsize != sizeof(num))
		return -1;
	memcpy(&num, key.dptr, key.dsize);
	if (num >= NUM_TESTS)
		return -1;
	if (found[num])
		return -1;
	found[num] = true;
	return 0;
}

static bool is_all_set(bool found[], unsigned int num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		if (!found[i])
			return false;
	return true;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	bool found[NUM_TESTS];
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT
	};

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 6 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-93-repack.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			break;

		ok1(store_all(ntdb));

		ok1(ntdb_repack(ntdb) == NTDB_SUCCESS);
		memset(found, 0, sizeof(found));
		ok1(ntdb_check(ntdb, NULL, NULL) == NTDB_SUCCESS);
		ok1(ntdb_traverse(ntdb, mark_entry, found) == NUM_TESTS);
		ok1(is_all_set(found, NUM_TESTS));
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
