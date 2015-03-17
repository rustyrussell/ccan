#include "private.h"
#include <unistd.h>
#include "lock-tracking.h"

#define fcntl fcntl_with_lockcheck
#include "ntdb-source.h"

#include "tap-interface.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include "external-agent.h"
#include "logging.h"

#define TEST_DBNAME "run-lockall.ntdb"
#define KEY_STR "key"

#undef fcntl

int main(int argc, char *argv[])
{
	struct agent *agent;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	int i;

	plan_tests(13 * sizeof(flags)/sizeof(flags[0]) + 1);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		enum agent_return ret;
		struct ntdb_context *ntdb;

		ntdb = ntdb_open(TEST_DBNAME, flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);

		ret = external_agent_operation(agent, OPEN, TEST_DBNAME);
		ok1(ret == SUCCESS);

		ok1(ntdb_lockall(ntdb) == NTDB_SUCCESS);
		ok1(external_agent_operation(agent, STORE, KEY_STR "=" KEY_STR)
		    == WOULD_HAVE_BLOCKED);
		ok1(external_agent_operation(agent, FETCH, KEY_STR "=" KEY_STR)
		    == WOULD_HAVE_BLOCKED);
		/* Test nesting. */
		ok1(ntdb_lockall(ntdb) == NTDB_SUCCESS);
		ntdb_unlockall(ntdb);
		ntdb_unlockall(ntdb);

		ok1(external_agent_operation(agent, STORE, KEY_STR "=" KEY_STR)
		    == SUCCESS);

		ok1(ntdb_lockall_read(ntdb) == NTDB_SUCCESS);
		ok1(external_agent_operation(agent, STORE, KEY_STR "=" KEY_STR)
		    == WOULD_HAVE_BLOCKED);
		ok1(external_agent_operation(agent, FETCH, KEY_STR "=" KEY_STR)
		    == SUCCESS);
		ok1(ntdb_lockall_read(ntdb) == NTDB_SUCCESS);
		ntdb_unlockall_read(ntdb);
		ntdb_unlockall_read(ntdb);

		ok1(external_agent_operation(agent, STORE, KEY_STR "=" KEY_STR)
		    == SUCCESS);
		ok1(external_agent_operation(agent, CLOSE, NULL) == SUCCESS);
		ntdb_close(ntdb);
	}

	free_external_agent(agent);
	ok1(tap_log_messages == 0);
	return exit_status();
}
