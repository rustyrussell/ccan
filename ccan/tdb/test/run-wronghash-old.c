#define _XOPEN_SOURCE 500
#include <ccan/tdb/tdb.h>
#include <ccan/tdb/io.c>
#include <ccan/tdb/tdb.c>
#include <ccan/tdb/lock.c>
#include <ccan/tdb/freelist.c>
#include <ccan/tdb/traverse.c>
#include <ccan/tdb/transaction.c>
#include <ccan/tdb/error.c>
#include <ccan/tdb/open.c>
#include <ccan/tdb/check.c>
#include <ccan/hash/hash.h>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static unsigned int non_jenkins_hash(TDB_DATA *key)
{
	return ~hash_stable(key->dptr, key->dsize, 0);
}

static void log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...)
{
	unsigned int *count = tdb_get_logging_private(tdb);
	/* Old code used to complain about spinlocks on new databases. */
	if (strstr(fmt, "spinlock"))
		(*count)++;
}

/* The old code should barf on new-style TDBs created with a non-default hash.
 */
int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	unsigned int log_count;
	struct tdb_logging_context log_ctx = { log_fn, &log_count };

	plan_tests(8);

	/* We should fail to open new-style non-default-hash tdbs of
	 * either endian. */
	log_count = 0;
	tdb = tdb_open_ex("test/jenkins-le-hash.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb_open_ex("test/jenkins-be-hash.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(!tdb);
	ok1(log_count == 1);

	/* And of course, if we use the wrong hash it will still fail. */
	log_count = 0;
	tdb = tdb_open_ex("test/jenkins-le-hash.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, non_jenkins_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb_open_ex("test/jenkins-be-hash.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, non_jenkins_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	return exit_status();
}
