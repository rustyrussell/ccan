#include "config.h"
#include <unistd.h>
#include "lock-tracking.h"

#define fcntl fcntl_with_lockcheck

#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include "external-agent.h"
#include "logging.h"

#define TEST_DBNAME "run-lockall.tdb"

#undef fcntl

int main(int argc, char *argv[])
{
	struct agent *agent;
	const int flags[] = { TDB_DEFAULT,
			      TDB_NOMMAP,
			      TDB_CONVERT,
			      TDB_CONVERT | TDB_NOMMAP };
	int i;

	plan_tests(13 * sizeof(flags)/sizeof(flags[0]) + 1);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		enum agent_return ret;
		struct tdb_context *tdb;

		tdb = tdb_open(TEST_DBNAME, flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);

		ret = external_agent_operation(agent, OPEN, TEST_DBNAME);
		ok1(ret == SUCCESS);

		ok1(tdb_lockall(tdb) == TDB_SUCCESS);
		ok1(external_agent_operation(agent, STORE, "key")
		    == WOULD_HAVE_BLOCKED);
		ok1(external_agent_operation(agent, FETCH, "key")
		    == WOULD_HAVE_BLOCKED);
		/* Test nesting. */
		ok1(tdb_lockall(tdb) == TDB_SUCCESS);
		tdb_unlockall(tdb);
		tdb_unlockall(tdb);

		ok1(external_agent_operation(agent, STORE, "key") == SUCCESS);

		ok1(tdb_lockall_read(tdb) == TDB_SUCCESS);
		ok1(external_agent_operation(agent, STORE, "key")
		    == WOULD_HAVE_BLOCKED);
		ok1(external_agent_operation(agent, FETCH, "key") == SUCCESS);
		ok1(tdb_lockall_read(tdb) == TDB_SUCCESS);
		tdb_unlockall_read(tdb);
		tdb_unlockall_read(tdb);

		ok1(external_agent_operation(agent, STORE, "key") == SUCCESS);
		ok1(external_agent_operation(agent, CLOSE, NULL) == SUCCESS);
		tdb_close(tdb);
	}

	free_external_agent(agent);
	ok1(tap_log_messages == 0);
	return exit_status();
}
