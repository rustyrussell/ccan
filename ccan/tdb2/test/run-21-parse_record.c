#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

static enum TDB_ERROR parse(TDB_DATA key, TDB_DATA data, TDB_DATA *expected)
{
	if (!tdb_deq(data, *expected))
		return TDB_ERR_EINVAL;
	return TDB_SUCCESS;
}

static enum TDB_ERROR parse_err(TDB_DATA key, TDB_DATA data, void *unused)
{
	return 100;
}

static bool test_records(struct tdb_context *tdb)
{
	int i;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < 1000; i++) {
		if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
			return false;
	}

	for (i = 0; i < 1000; i++) {
		if (tdb_parse_record(tdb, key, parse, &data) != TDB_SUCCESS)
			return false;
	}

	if (tdb_parse_record(tdb, key, parse, &data) != TDB_ERR_NOEXIST)
		return false;

	/* Test error return from parse function. */
	i = 0;
	if (tdb_parse_record(tdb, key, parse_err, NULL) != 100)
		return false;

	return true;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 2 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-14-exists.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (ok1(tdb))
			ok1(test_records(tdb));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
