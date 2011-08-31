#include <ccan/tdb2/private.h>
#include <unistd.h>
#include "tdb1-lock-tracking.h"

#define fcntl fcntl_with_lockcheck1

#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

#undef fcntl

#define NUM_ENTRIES 10

static bool prepare_entries(struct tdb1_context *tdb)
{
	unsigned int i;
	TDB1_DATA key, data;

	for (i = 0; i < NUM_ENTRIES; i++) {
		key.dsize = sizeof(i);
		key.dptr = (void *)&i;
		data.dsize = strlen("world");
		data.dptr = (void *)"world";

		if (tdb1_store(tdb, key, data, 0) != 0)
			return false;
	}
	return true;
}

static void delete_entries(struct tdb1_context *tdb)
{
	unsigned int i;
	TDB1_DATA key;

	for (i = 0; i < NUM_ENTRIES; i++) {
		key.dsize = sizeof(i);
		key.dptr = (void *)&i;

		ok1(tdb1_delete(tdb, key) == 0);
	}
}

/* We don't know how many times this will run. */
static int delete_other(struct tdb1_context *tdb, TDB1_DATA key, TDB1_DATA data,
			void *private_data)
{
	unsigned int i;
	memcpy(&i, key.dptr, 4);
	i = (i + 1) % NUM_ENTRIES;
	key.dptr = (void *)&i;
	if (tdb1_delete(tdb, key) != 0)
		(*(int *)private_data)++;
	return 0;
}

static int delete_self(struct tdb1_context *tdb, TDB1_DATA key, TDB1_DATA data,
			void *private_data)
{
	ok1(tdb1_delete(tdb, key) == 0);
	return 0;
}

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	int errors = 0;

	plan_tests(41);
	tdb = tdb1_open_ex("run-no-lock-during-traverse.tdb",
			  1024, TDB1_CLEAR_IF_FIRST, O_CREAT|O_TRUNC|O_RDWR,
			  0600, &taplogctx, NULL);

	ok1(tdb);
	ok1(prepare_entries(tdb));
	ok1(locking_errors1 == 0);
	ok1(tdb1_lockall(tdb) == 0);
	ok1(locking_errors1 == 0);
	tdb1_traverse(tdb, delete_other, &errors);
	ok1(errors == 0);
	ok1(locking_errors1 == 0);
	ok1(tdb1_unlockall(tdb) == 0);

	ok1(prepare_entries(tdb));
	ok1(locking_errors1 == 0);
	ok1(tdb1_lockall(tdb) == 0);
	ok1(locking_errors1 == 0);
	tdb1_traverse(tdb, delete_self, NULL);
	ok1(locking_errors1 == 0);
	ok1(tdb1_unlockall(tdb) == 0);

	ok1(prepare_entries(tdb));
	ok1(locking_errors1 == 0);
	ok1(tdb1_lockall(tdb) == 0);
	ok1(locking_errors1 == 0);
	delete_entries(tdb);
	ok1(locking_errors1 == 0);
	ok1(tdb1_unlockall(tdb) == 0);

	ok1(tdb1_close(tdb) == 0);

	return exit_status();
}
