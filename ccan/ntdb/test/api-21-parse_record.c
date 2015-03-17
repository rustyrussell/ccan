#include "config.h"
#include "ntdb.h"
#include "private.h"
#include "tap-interface.h"
#include "logging.h"

static enum NTDB_ERROR parse(NTDB_DATA key, NTDB_DATA data, NTDB_DATA *expected)
{
	if (!ntdb_deq(data, *expected))
		return NTDB_ERR_EINVAL;
	return NTDB_SUCCESS;
}

static enum NTDB_ERROR parse_err(NTDB_DATA key, NTDB_DATA data, void *unused)
{
	return 100;
}

static bool test_records(struct ntdb_context *ntdb)
{
	int i;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < 1000; i++) {
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
			return false;
	}

	for (i = 0; i < 1000; i++) {
		if (ntdb_parse_record(ntdb, key, parse, &data) != NTDB_SUCCESS)
			return false;
	}

	if (ntdb_parse_record(ntdb, key, parse, &data) != NTDB_ERR_NOEXIST)
		return false;

	/* Test error return from parse function. */
	i = 0;
	if (ntdb_parse_record(ntdb, key, parse_err, NULL) != 100)
		return false;

	return true;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 2 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-21-parse_record.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (ok1(ntdb))
			ok1(test_records(ntdb));
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
