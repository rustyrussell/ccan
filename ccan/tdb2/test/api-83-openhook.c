#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include <unistd.h>
#include "external-agent.h"
#include "logging.h"

static enum TDB_ERROR clear_if_first(int fd, void *arg)
{
/* We hold a lock offset 4 always, so we can tell if anyone is holding it.
 * (This is compatible with tdb1's TDB_CLEAR_IF_FIRST flag).  */
	struct flock fl;

	if (arg != clear_if_first)
		return TDB_ERR_CORRUPT;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 4;
	fl.l_len = 1;

	if (fcntl(fd, F_SETLK, &fl) == 0) {
		/* We must be first ones to open it! */
		diag("truncating file!");
		if (ftruncate(fd, 0) != 0) {
			return TDB_ERR_IO;
		}
	}
	fl.l_type = F_RDLCK;
	if (fcntl(fd, F_SETLKW, &fl) != 0) {
		return TDB_ERR_IO;
	}
	return TDB_SUCCESS;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	struct agent *agent;
	union tdb_attribute cif;
	struct tdb_data key = tdb_mkdata("key", 3);
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
			TDB_VERSION1, TDB_NOMMAP|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	cif.openhook.base.attr = TDB_ATTRIBUTE_OPENHOOK;
	cif.openhook.base.next = &tap_log_attr;
	cif.openhook.fn = clear_if_first;
	cif.openhook.data = clear_if_first;

	agent = prepare_external_agent();
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 13);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		/* Create it */
		tdb = tdb_open("run-83-openhook.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		ok1(tdb);
		ok1(tdb_store(tdb, key, key, TDB_REPLACE) == 0);
		tdb_close(tdb);

		/* Now, open with CIF, should clear it. */
		tdb = tdb_open("run-83-openhook.tdb", flags[i],
			       O_RDWR, 0, &cif);
		ok1(tdb);
		ok1(!tdb_exists(tdb, key));
		ok1(tdb_store(tdb, key, key, TDB_REPLACE) == 0);

		/* Agent should not clear it, since it's still open. */
		ok1(external_agent_operation(agent, OPEN_WITH_HOOK,
					     "run-83-openhook.tdb") == SUCCESS);
		ok1(external_agent_operation(agent, FETCH, "key") == SUCCESS);
		ok1(external_agent_operation(agent, CLOSE, "") == SUCCESS);

		/* Still exists for us too. */
		ok1(tdb_exists(tdb, key));

		/* Close it, now agent should clear it. */
		tdb_close(tdb);

		ok1(external_agent_operation(agent, OPEN_WITH_HOOK,
					     "run-83-openhook.tdb") == SUCCESS);
		ok1(external_agent_operation(agent, FETCH, "key") == FAILED);
		ok1(external_agent_operation(agent, CLOSE, "") == SUCCESS);

		ok1(tap_log_messages == 0);
	}

	free_external_agent(agent);
	return exit_status();
}
