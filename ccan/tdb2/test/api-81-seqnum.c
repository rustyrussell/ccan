#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i, seq;
	struct tdb_context *tdb;
	struct tdb_data d = { NULL, 0 }; /* Bogus GCC warning */
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 15 + 8 * 13);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("api-81-seqnum.tdb", flags[i]|TDB_SEQNUM,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			continue;

		seq = 0;
		ok1(tdb_get_seqnum(tdb) == seq);
		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_get_seqnum(tdb) == ++seq);
		/* Fetch doesn't change seqnum */
		if (ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS))
			free(d.dptr);
		ok1(tdb_get_seqnum(tdb) == seq);
		ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
		/* Append in tdb1 (or store over value) bumps twice! */
		if (flags[i] & TDB_VERSION1)
			seq++;
		ok1(tdb_get_seqnum(tdb) == ++seq);

		ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == ++seq);
		/* Empty append works */
		ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == ++seq);

		ok1(tdb_wipe_all(tdb) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == ++seq);

		if (!(flags[i] & TDB_INTERNAL)) {
			ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
			ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
			ok1(tdb_get_seqnum(tdb) == ++seq);
			/* Append in tdb1 (or store over value) bumps twice! */
			if (flags[i] & TDB_VERSION1)
				seq++;
			ok1(tdb_append(tdb, key, data) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == ++seq);
			ok1(tdb_delete(tdb, key) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == ++seq);
			ok1(tdb_transaction_commit(tdb) == TDB_SUCCESS);
			ok1(tdb_get_seqnum(tdb) == seq);

			ok1(tdb_transaction_start(tdb) == TDB_SUCCESS);
			ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
			ok1(tdb_get_seqnum(tdb) == seq + 1);
			tdb_transaction_cancel(tdb);
			ok1(tdb_get_seqnum(tdb) == seq);
		}
		tdb_close(tdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
