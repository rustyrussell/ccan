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
#include <ccan/tdb/hash.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static unsigned int tdb_dumb_hash(TDB_DATA *key)
{
	return key->dsize;
}

static void log_fn(struct tdb_context *tdb, enum tdb_debug_level level, const char *fmt, ...)
{
	unsigned int *count = tdb_get_logging_private(tdb);
	if (strstr(fmt, "hash"))
		(*count)++;
}

static unsigned int hdr_rwlocks(const char *fname)
{
	struct tdb_header hdr;

	int fd = open(fname, O_RDONLY);
	if (fd == -1)
		return -1;

	if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr))
		return -1;

	close(fd);
	return hdr.rwlocks;
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	unsigned int log_count, flags;
	TDB_DATA d;
	struct tdb_logging_context log_ctx = { log_fn, &log_count };

	plan_tests(38 * 2);

	for (flags = 0; flags <= TDB_CONVERT; flags += TDB_CONVERT) {
		unsigned int rwmagic = TDB_HASH_RWLOCK_MAGIC;

		if (flags & TDB_CONVERT)
			tdb_convert(&rwmagic, sizeof(rwmagic));

		/* Create an old-style hash. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, flags,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb_store(tdb, d, d, TDB_INSERT) == 0);
		tdb_close(tdb);

		/* Should not have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == 0);

		/* We can still open any old-style with incompat flag. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0,
				  TDB_INCOMPATIBLE_HASH,
				  O_RDWR, 0600, &log_ctx, NULL);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_fetch(tdb, d).dsize == 5);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);

		log_count = 0;
		tdb = tdb_open_ex("test/jenkins-le-hash.tdb", 0, 0, O_RDONLY,
				  0, &log_ctx, tdb_jenkins_hash);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);

		log_count = 0;
		tdb = tdb_open_ex("test/jenkins-be-hash.tdb", 0, 0, O_RDONLY,
				  0, &log_ctx, tdb_jenkins_hash);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);

		/* OK, now create with incompatible flag, default hash. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0,
				  flags|TDB_INCOMPATIBLE_HASH,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb_store(tdb, d, d, TDB_INSERT) == 0);
		tdb_close(tdb);

		/* Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == rwmagic);

		/* Cannot open with old hash. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, tdb_old_hash);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Can open with jenkins hash. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, tdb_jenkins_hash);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_fetch(tdb, d).dsize == 5);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);

		/* Can open by letting it figure it out itself. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, NULL);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_fetch(tdb, d).dsize == 5);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);

		/* We can also use incompatible hash with other hashes. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0,
				  flags|TDB_INCOMPATIBLE_HASH,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  tdb_dumb_hash);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb_store(tdb, d, d, TDB_INSERT) == 0);
		tdb_close(tdb);

		/* Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == rwmagic);

		/* It should not open if we don't specify. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, 0, O_RDWR, 0,
				  &log_ctx, NULL);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Should reopen with correct hash. */
		log_count = 0;
		tdb = tdb_open_ex("run-incompatible.tdb", 0, 0, O_RDWR, 0,
				  &log_ctx, tdb_dumb_hash);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb_fetch(tdb, d).dsize == 5);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		tdb_close(tdb);
	}

	return exit_status();
}
