/* Make sure write operations fail during ntdb_parse(). */
#include "config.h"
#include "ntdb.h"
#include "private.h"
#include "tap-interface.h"
#include "logging.h"

static struct ntdb_context *ntdb;

/* We could get either of these. */
static bool xfail(enum NTDB_ERROR ecode)
{
	return ecode == NTDB_ERR_RDONLY || ecode == NTDB_ERR_LOCK;
}

static enum NTDB_ERROR parse(NTDB_DATA key, NTDB_DATA data,
			     NTDB_DATA *expected)
{
	NTDB_DATA add = ntdb_mkdata("another", strlen("another"));

	if (!ntdb_deq(data, *expected)) {
		return NTDB_ERR_EINVAL;
	}

	/* These should all fail.*/
	if (!xfail(ntdb_store(ntdb, add, add, NTDB_INSERT))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_append(ntdb, key, add))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_delete(ntdb, key))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_transaction_start(ntdb))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_chainlock(ntdb, key))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_lockall(ntdb))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_wipe_all(ntdb))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	if (!xfail(ntdb_repack(ntdb))) {
		return NTDB_ERR_EINVAL;
	}
	tap_log_messages--;

	/* Access the record one more time. */
	if (!ntdb_deq(data, *expected)) {
		return NTDB_ERR_EINVAL;
	}

	return NTDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP, NTDB_CONVERT };
	NTDB_DATA key = ntdb_mkdata("hello", 5), data = ntdb_mkdata("world", 5);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 2 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-95-read-only-during-parse.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == NTDB_SUCCESS);
		ok1(ntdb_parse_record(ntdb, key, parse, &data) == NTDB_SUCCESS);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
