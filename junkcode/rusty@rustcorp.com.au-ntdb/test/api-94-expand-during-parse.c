/* We use direct access to hand to the parse function: what if db expands? */
#include "config.h"
#include "../ntdb.h"
#include "tap-interface.h"
#include "logging.h"
#include "../private.h" /* To establish size, esp. for NTDB_INTERNAL dbs */
#include "helpapi-external-agent.h"

static struct ntdb_context *ntdb;

static off_t ntdb_size(void)
{
	return ntdb->file->map_size;
}

struct parse_info {
	unsigned int depth;
	NTDB_DATA expected;
};

static enum NTDB_ERROR parse(NTDB_DATA key, NTDB_DATA data,
			     struct parse_info *pinfo)
{
	off_t flen;
	unsigned int i;

	if (!ntdb_deq(data, pinfo->expected))
		return NTDB_ERR_EINVAL;

	flen = ntdb_size();

	for (i = 0; ntdb_size() == flen; i++) {
		NTDB_DATA add = ntdb_mkdata(&i, sizeof(i));

		/* This is technically illegal parse(), which is why we
		 * grabbed allrecord lock.*/
		ntdb_store(ntdb, add, add, NTDB_INSERT);
	}

	/* Access the record again. */
	if (!ntdb_deq(data, pinfo->expected))
		return NTDB_ERR_EINVAL;

	/* Recurse!  Woot! */
	if (pinfo->depth != 0) {
		enum NTDB_ERROR ecode;

		pinfo->depth--;
		ecode = ntdb_parse_record(ntdb, key, parse, pinfo);
		if (ecode) {
			return ecode;
		}
	}

	/* Access the record one more time. */
	if (!ntdb_deq(data, pinfo->expected))
		return NTDB_ERR_EINVAL;

	return NTDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	struct parse_info pinfo;
	NTDB_DATA key = ntdb_mkdata("hello", 5), data = ntdb_mkdata("world", 5);

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-94-expand-during-parse.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == NTDB_SUCCESS);
		ok1(ntdb_lockall(ntdb) == NTDB_SUCCESS);
		pinfo.expected = data;
		pinfo.depth = 3;
		ok1(ntdb_parse_record(ntdb, key, parse, &pinfo) == NTDB_SUCCESS);
		ntdb_unlockall(ntdb);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
