#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include <stdlib.h>
#include "logging.h"
#include "helpapi-external-agent.h"

int main(int argc, char *argv[])
{
	unsigned int i, seq;
	struct ntdb_context *ntdb;
	NTDB_DATA d = { NULL, 0 }; /* Bogus GCC warning */
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 15 + 4 * 13);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-81-seqnum.ntdb",
				 flags[i]|NTDB_SEQNUM|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(ntdb))
			continue;

		seq = 0;
		ok1(ntdb_get_seqnum(ntdb) == seq);
		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb_get_seqnum(ntdb) == ++seq);
		/* Fetch doesn't change seqnum */
		if (ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS))
			free(d.dptr);
		ok1(ntdb_get_seqnum(ntdb) == seq);
		ok1(ntdb_append(ntdb, key, data) == NTDB_SUCCESS);
		ok1(ntdb_get_seqnum(ntdb) == ++seq);

		ok1(ntdb_delete(ntdb, key) == NTDB_SUCCESS);
		ok1(ntdb_get_seqnum(ntdb) == ++seq);
		/* Empty append works */
		ok1(ntdb_append(ntdb, key, data) == NTDB_SUCCESS);
		ok1(ntdb_get_seqnum(ntdb) == ++seq);

		ok1(ntdb_wipe_all(ntdb) == NTDB_SUCCESS);
		ok1(ntdb_get_seqnum(ntdb) == ++seq);

		if (!(flags[i] & NTDB_INTERNAL)) {
			ok1(ntdb_transaction_start(ntdb) == NTDB_SUCCESS);
			ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
			ok1(ntdb_get_seqnum(ntdb) == ++seq);
			ok1(ntdb_append(ntdb, key, data) == NTDB_SUCCESS);
			ok1(ntdb_get_seqnum(ntdb) == ++seq);
			ok1(ntdb_delete(ntdb, key) == NTDB_SUCCESS);
			ok1(ntdb_get_seqnum(ntdb) == ++seq);
			ok1(ntdb_transaction_commit(ntdb) == NTDB_SUCCESS);
			ok1(ntdb_get_seqnum(ntdb) == seq);

			ok1(ntdb_transaction_start(ntdb) == NTDB_SUCCESS);
			ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
			ok1(ntdb_get_seqnum(ntdb) == seq + 1);
			ntdb_transaction_cancel(ntdb);
			ok1(ntdb_get_seqnum(ntdb) == seq);
		}
		ntdb_close(ntdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
