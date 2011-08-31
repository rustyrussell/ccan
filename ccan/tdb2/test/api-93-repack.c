#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "logging.h"

#define NUM_TESTS 50000

static bool store_all(struct tdb_context *tdb)
{
	unsigned int i;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data dbuf = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_TESTS; i++) {
		if (tdb_store(tdb, key, dbuf, TDB_INSERT) != TDB_SUCCESS)
			return false;
	}
	return true;
}

static int mark_entry(struct tdb_context *tdb,
		      TDB_DATA key, TDB_DATA data, bool found[])
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
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
	};

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 6 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-93-repack.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			break;

		ok1(store_all(tdb));

		ok1(tdb_repack(tdb) == TDB_SUCCESS);
		memset(found, 0, sizeof(found));
		ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);
		ok1(tdb_traverse(tdb, mark_entry, found) == NUM_TESTS);
		ok1(is_all_set(found, NUM_TESTS));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
