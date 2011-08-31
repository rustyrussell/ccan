#include "tdb1-lock-tracking.h"
#define fcntl fcntl_with_lockcheck1
#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#undef fcntl
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include "tdb1-external-agent.h"
#include "tdb1-logging.h"

static struct agent *agent;

static bool correct_key(TDB_DATA key)
{
	return key.dsize == strlen("hi")
		&& memcmp(key.dptr, "hi", key.dsize) == 0;
}

static bool correct_data(TDB_DATA data)
{
	return data.dsize == strlen("world")
		&& memcmp(data.dptr, "world", data.dsize) == 0;
}

static int traverse2(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA data,
		     void *p)
{
	ok1(correct_key(key));
	ok1(correct_data(data));
	return 0;
}

static int traverse1(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA data,
		     void *p)
{
	ok1(correct_key(key));
	ok1(correct_data(data));
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == WOULD_HAVE_BLOCKED);
	tdb1_traverse(tdb, traverse2, NULL);

	/* That should *not* release the transaction lock! */
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == WOULD_HAVE_BLOCKED);
	return 0;
}

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB_DATA key, data;

	plan_tests(17);
	agent = prepare_external_agent1();
	if (!agent)
		err(1, "preparing agent");

	tdb = tdb1_open_ex("run-nested-traverse.tdb", 1024, TDB1_CLEAR_IF_FIRST,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &taplogctx, NULL);
	ok1(tdb);

	ok1(external_agent_operation1(agent, OPEN, tdb->name) == SUCCESS);
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == SUCCESS);
	ok1(external_agent_operation1(agent, TRANSACTION_COMMIT, tdb->name)
	    == SUCCESS);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dptr = (void *)"world";
	data.dsize = strlen("world");

	ok1(tdb1_store(tdb, key, data, TDB_INSERT) == 0);
	tdb1_traverse(tdb, traverse1, NULL);
	tdb1_traverse_read(tdb, traverse1, NULL);
	tdb1_close(tdb);

	return exit_status();
}
