#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static unsigned int tdb1_dumb_hash(TDB1_DATA *key)
{
	return key->dsize;
}

static void log_fn(struct tdb1_context *tdb, enum tdb_log_level level,
		   enum TDB_ERROR ecode, const char *message, void *priv)
{
	unsigned int *count = priv;
	if (strstr(message, "hash"))
		(*count)++;
}

static unsigned int hdr_rwlocks(const char *fname)
{
	struct tdb1_header hdr;

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
	struct tdb1_context *tdb;
	unsigned int log_count, flags;
	TDB1_DATA d;
	struct tdb1_logging_context log_ctx = { log_fn, &log_count };

	plan_tests(38 * 2);

	for (flags = 0; flags <= TDB1_CONVERT; flags += TDB1_CONVERT) {
		unsigned int rwmagic = TDB1_HASH_RWLOCK_MAGIC;

		if (flags & TDB1_CONVERT)
			tdb1_convert(&rwmagic, sizeof(rwmagic));

		/* Create an old-style hash. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, flags,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB1_INSERT) == 0);
		tdb1_close(tdb);

		/* Should not have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == 0);

		/* We can still open any old-style with incompat flag. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0,
				  TDB1_INCOMPATIBLE_HASH,
				  O_RDWR, 0600, &log_ctx, NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

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

		/* OK, now create with incompatible flag, default hash. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0,
				  flags|TDB1_INCOMPATIBLE_HASH,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB1_INSERT) == 0);
		tdb1_close(tdb);

		/* Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == rwmagic);

		/* Cannot open with old hash. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, tdb1_old_hash);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Can open with jenkins hash. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, tdb1_jenkins_hash);
		ok1(tdb);
		ok1(log_count == 0);
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		/* Can open by letting it figure it out itself. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, 0,
				  O_RDWR, 0600, &log_ctx, NULL);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		/* We can also use incompatible hash with other hashes. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0,
				  flags|TDB1_INCOMPATIBLE_HASH,
				  O_CREAT|O_RDWR|O_TRUNC, 0600, &log_ctx,
				  tdb1_dumb_hash);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB1_INSERT) == 0);
		tdb1_close(tdb);

		/* Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == rwmagic);

		/* It should not open if we don't specify. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, 0, O_RDWR, 0,
				  &log_ctx, NULL);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Should reopen with correct hash. */
		log_count = 0;
		tdb = tdb1_open_ex("run-incompatible.tdb", 0, 0, O_RDWR, 0,
				  &log_ctx, tdb1_dumb_hash);
		ok1(tdb);
		ok1(log_count == 0);
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);
	}

	return exit_status();
}
