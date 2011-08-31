#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static void log_fn(struct tdb1_context *tdb, enum tdb_log_level level,
		   enum TDB_ERROR ecode, const char *message, void *priv)
{
	unsigned int *count = priv;
	if (strstr(message, "hash"))
		(*count)++;
}

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	unsigned int log_count;
	TDB_DATA d;
	struct tdb1_logging_context log_ctx = { log_fn, &log_count };

	plan_tests(28);

	/* Create with default hash. */
	log_count = 0;
	tdb = tdb1_open_ex("run-wronghash-fail.tdb", 0, 0,
			  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx, NULL);
	ok1(tdb);
	ok1(log_count == 0);
	d.dptr = (void *)"Hello";
	d.dsize = 5;
	ok1(tdb1_store(tdb, d, d, TDB_INSERT) == 0);
	tdb1_close(tdb);

	/* Fail to open with different hash. */
	tdb = tdb1_open_ex("run-wronghash-fail.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, tdb1_jenkins_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	/* Create with different hash. */
	log_count = 0;
	tdb = tdb1_open_ex("run-wronghash-fail.tdb", 0, 0,
			  O_CREAT|O_RDWR|O_TRUNC,
			  0600, &log_ctx, tdb1_jenkins_hash);
	ok1(tdb);
	ok1(log_count == 0);
	tdb1_close(tdb);

	/* Endian should be no problem. */
	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-le-hash.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, tdb1_old_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-be-hash.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, tdb1_old_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	/* Fail to open with old default hash. */
	tdb = tdb1_open_ex("run-wronghash-fail.tdb", 0, 0, O_RDWR, 0,
			  &log_ctx, tdb1_old_hash);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-le-hash.tdb1", 0, 0, O_RDONLY,
			  0, &log_ctx, tdb1_jenkins_hash);
	ok1(tdb);
	ok1(log_count == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-be-hash.tdb1", 0, 0, O_RDONLY,
			  0, &log_ctx, tdb1_jenkins_hash);
	ok1(tdb);
	ok1(log_count == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	/* It should open with jenkins hash if we don't specify. */
	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-le-hash.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(tdb);
	ok1(log_count == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	log_count = 0;
	tdb = tdb1_open_ex("test/jenkins-be-hash.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(tdb);
	ok1(log_count == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);

	log_count = 0;
	tdb = tdb1_open_ex("run-wronghash-fail.tdb", 0, 0, O_RDONLY,
			  0, &log_ctx, NULL);
	ok1(tdb);
	ok1(log_count == 0);
	ok1(tdb1_check(tdb, NULL, NULL) == 0);
	tdb1_close(tdb);


	return exit_status();
}
