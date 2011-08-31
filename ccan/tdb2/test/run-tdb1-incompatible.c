#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static uint64_t tdb1_dumb_hash(const void *key, size_t len, uint64_t seed,
			       void *unused)
{
	return len;
}

static void log_fn(struct tdb_context *tdb, enum tdb_log_level level,
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

static uint64_t jenkins_hashfn(const void *key, size_t len, uint64_t seed,
			       void *unused)
{
	return hashlittle(key, len);
}

static uint64_t old_hash(const void *key, size_t len, uint64_t seed,
			 void *unused)
{
	return tdb1_old_hash(key, len, seed, unused);
}

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	unsigned int log_count, flags;
	TDB_DATA d;
	union tdb_attribute log_attr, jhash_attr, ohash_attr,
		incompat_hash_attr, dumbhash_attr;

	log_attr.base.attr = TDB_ATTRIBUTE_LOG;
	log_attr.base.next = NULL;
	log_attr.log.fn = log_fn;
	log_attr.log.data = &log_count;

	jhash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	jhash_attr.base.next = &log_attr;
	jhash_attr.hash.fn = jenkins_hashfn;

	ohash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	ohash_attr.base.next = &log_attr;
	ohash_attr.hash.fn = old_hash;

	incompat_hash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	incompat_hash_attr.base.next = &log_attr;
	incompat_hash_attr.hash.fn = tdb1_incompatible_hash;

	dumbhash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	dumbhash_attr.base.next = &log_attr;
	dumbhash_attr.hash.fn = tdb1_dumb_hash;

	plan_tests(38 * 2);

	for (flags = 0; flags <= TDB_CONVERT; flags += TDB_CONVERT) {
		unsigned int rwmagic = TDB1_HASH_RWLOCK_MAGIC;

		if (flags & TDB_CONVERT)
			tdb1_convert(&rwmagic, sizeof(rwmagic));

		/* Create an old-style hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", flags,
				O_CREAT|O_RDWR|O_TRUNC, 0600, &log_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB_INSERT) == 0);
		tdb1_close(tdb);

		/* Should not have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == 0);

		/* We can still open any old-style with incompat hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb",
				TDB_DEFAULT,
				O_RDWR, 0600, &incompat_hash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		log_count = 0;
		tdb = tdb1_open("test/jenkins-le-hash.tdb1", 0, O_RDONLY,
				0, &jhash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		log_count = 0;
		tdb = tdb1_open("test/jenkins-be-hash.tdb1", 0, O_RDONLY,
				0, &jhash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		/* OK, now create with incompatible hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb",
				flags,
				O_CREAT|O_RDWR|O_TRUNC, 0600,
				&incompat_hash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB_INSERT) == 0);
		tdb1_close(tdb);

		/* Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") == rwmagic);

		/* Cannot open with old hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", 0,
				O_RDWR, 0600, &ohash_attr);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Can open with jenkins hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", 0,
				O_RDWR, 0600, &jhash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		/* Can open by letting it figure it out itself. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", 0,
				O_RDWR, 0600, &log_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		d = tdb1_fetch(tdb, d);
		ok1(d.dsize == 5);
		free(d.dptr);
		ok1(tdb1_check(tdb, NULL, NULL) == 0);
		tdb1_close(tdb);

		/* FIXME: Not possible with TDB2 :( */
		/* We can also use incompatible hash with other hashes. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb",
				flags,
				O_CREAT|O_RDWR|O_TRUNC, 0600, &dumbhash_attr);
		ok1(tdb);
		ok1(log_count == 0);
		d.dptr = (void *)"Hello";
		d.dsize = 5;
		ok1(tdb1_store(tdb, d, d, TDB_INSERT) == 0);
		tdb1_close(tdb);

		/* FIXME: Should have marked rwlocks field. */
		ok1(hdr_rwlocks("run-incompatible.tdb") != rwmagic);

		/* It should not open if we don't specify. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", 0, O_RDWR, 0,
				&log_attr);
		ok1(!tdb);
		ok1(log_count == 1);

		/* Should reopen with correct hash. */
		log_count = 0;
		tdb = tdb1_open("run-incompatible.tdb", 0, O_RDWR, 0,
				&dumbhash_attr);
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
