#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "external-agent.h"
#include "logging.h"
#include "helpapi-external-agent.h"

#define KEY_STR "key"

static enum NTDB_ERROR clear_if_first(int fd, void *arg)
{
/* We hold a lock offset 4 always, so we can tell if anyone is holding it.
 * (This is compatible with tdb's TDB_CLEAR_IF_FIRST flag).  */
	struct flock fl;

	if (arg != clear_if_first)
		return NTDB_ERR_CORRUPT;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 4;
	fl.l_len = 1;

	if (fcntl(fd, F_SETLK, &fl) == 0) {
		/* We must be first ones to open it! */
		diag("truncating file!");
		if (ftruncate(fd, 0) != 0) {
			return NTDB_ERR_IO;
		}
	}
	fl.l_type = F_RDLCK;
	if (fcntl(fd, F_SETLKW, &fl) != 0) {
		return NTDB_ERR_IO;
	}
	return NTDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb, *ntdb2;
	struct agent *agent;
	union ntdb_attribute cif;
	NTDB_DATA key = ntdb_mkdata(KEY_STR, strlen(KEY_STR));
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };

	cif.openhook.base.attr = NTDB_ATTRIBUTE_OPENHOOK;
	cif.openhook.base.next = &tap_log_attr;
	cif.openhook.fn = clear_if_first;
	cif.openhook.data = clear_if_first;

	agent = prepare_external_agent();
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 16);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		/* Create it */
		ntdb = ntdb_open("run-83-openhook.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		ok1(ntdb);
		ok1(ntdb_store(ntdb, key, key, NTDB_REPLACE) == 0);
		ntdb_close(ntdb);

		/* Now, open with CIF, should clear it. */
		ntdb = ntdb_open("run-83-openhook.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR, 0, &cif);
		ok1(ntdb);
		ok1(!ntdb_exists(ntdb, key));
		ok1(ntdb_store(ntdb, key, key, NTDB_REPLACE) == 0);

		/* Agent should not clear it, since it's still open. */
		ok1(external_agent_operation(agent, OPEN_WITH_HOOK,
					     "run-83-openhook.ntdb") == SUCCESS);
		ok1(external_agent_operation(agent, FETCH, KEY_STR "=" KEY_STR)
		    == SUCCESS);
		ok1(external_agent_operation(agent, CLOSE, "") == SUCCESS);

		/* Still exists for us too. */
		ok1(ntdb_exists(ntdb, key));

		/* Nested open should not erase db. */
		ntdb2 = ntdb_open("run-83-openhook.ntdb", flags[i]|MAYBE_NOSYNC,
				  O_RDWR, 0, &cif);
		ok1(ntdb_exists(ntdb2, key));
		ok1(ntdb_exists(ntdb, key));
		ntdb_close(ntdb2);

		ok1(ntdb_exists(ntdb, key));

		/* Close it, now agent should clear it. */
		ntdb_close(ntdb);

		ok1(external_agent_operation(agent, OPEN_WITH_HOOK,
					     "run-83-openhook.ntdb") == SUCCESS);
		ok1(external_agent_operation(agent, FETCH, KEY_STR "=" KEY_STR)
		    == FAILED);
		ok1(external_agent_operation(agent, CLOSE, "") == SUCCESS);

		ok1(tap_log_messages == 0);
	}

	free_external_agent(agent);
	return exit_status();
}
