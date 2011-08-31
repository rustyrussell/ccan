#include "config.h"
#include "tdb1-lock-tracking.h"
#define fcntl fcntl_with_lockcheck1
#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#undef fcntl_with_lockcheck
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

static int traverse(struct tdb1_context *tdb, TDB_DATA key, TDB_DATA data,
		     void *p)
{
	ok1(correct_key(key));
	ok1(correct_data(data));
	return 0;
}

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	TDB_DATA key, data;

	plan_tests(13);
	agent = prepare_external_agent1();
	if (!agent)
		err(1, "preparing agent");

	tdb = tdb1_open_ex("run-traverse-in-transaction.tdb",
			  1024, TDB_DEFAULT, O_CREAT|O_TRUNC|O_RDWR,
			  0600, &taplogctx, NULL);
	ok1(tdb);

	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dptr = (void *)"world";
	data.dsize = strlen("world");

	ok1(tdb1_store(tdb, key, data, TDB_INSERT) == 0);

	ok1(external_agent_operation1(agent, OPEN, tdb->name) == SUCCESS);

	ok1(tdb1_transaction_start(tdb) == 0);
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == WOULD_HAVE_BLOCKED);
	tdb1_traverse(tdb, traverse, NULL);

	/* That should *not* release the transaction lock! */
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == WOULD_HAVE_BLOCKED);
	tdb1_traverse_read(tdb, traverse, NULL);

	/* That should *not* release the transaction lock! */
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == WOULD_HAVE_BLOCKED);
	ok1(tdb1_transaction_commit(tdb) == 0);
	/* Now we should be fine. */
	ok1(external_agent_operation1(agent, TRANSACTION_START, tdb->name)
	    == SUCCESS);

	tdb1_close(tdb);

	return exit_status();
}
