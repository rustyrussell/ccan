#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tap/tap.h>
#include "logging.h"

static int mylock(int fd, int rw, off_t off, off_t len, bool waitflag,
		  void *unused)
{
	return 0;
}

static int myunlock(int fd, int rw, off_t off, off_t len, void *unused)
{
	return 0;
}

static uint64_t hash_fn(const void *key, size_t len, uint64_t seed,
			void *priv)
{
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };
	union tdb_attribute seed_attr;
	union tdb_attribute hash_attr;
	union tdb_attribute lock_attr;

	hash_attr.base.attr = TDB_ATTRIBUTE_HASH;
	hash_attr.base.next = &seed_attr;
	hash_attr.hash.fn = hash_fn;
	hash_attr.hash.data = &hash_attr;

	seed_attr.base.attr = TDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &lock_attr;
	seed_attr.seed.seed = 100;

	lock_attr.base.attr = TDB_ATTRIBUTE_FLOCK;
	lock_attr.base.next = &tap_log_attr;
	lock_attr.flock.lock = mylock;
	lock_attr.flock.unlock = myunlock;
	lock_attr.flock.data = &lock_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 50);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		union tdb_attribute attr;

		/* First open with no attributes. */
		tdb = tdb_open("run-90-get-set-attributes.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		ok1(tdb);

		/* Get log on no attributes will fail */
		attr.base.attr = TDB_ATTRIBUTE_LOG;
		ok1(tdb_get_attribute(tdb, &attr) == TDB_ERR_NOEXIST);
		/* These always work. */
		attr.base.attr = TDB_ATTRIBUTE_HASH;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_HASH);
		ok1(attr.hash.fn == jenkins_hash);
		attr.base.attr = TDB_ATTRIBUTE_FLOCK;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == tdb_fcntl_lock);
		ok1(attr.flock.unlock == tdb_fcntl_unlock);
		attr.base.attr = TDB_ATTRIBUTE_SEED;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_SEED);
		/* This is possible, just astronomically unlikely. */
		ok1(attr.seed.seed != 0);

		/* Unset attributes. */
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_LOG);
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_FLOCK);

		/* Set them. */
		ok1(tdb_set_attribute(tdb, &tap_log_attr) == 0);
		ok1(tdb_set_attribute(tdb, &lock_attr) == 0);
		/* These should fail. */
		ok1(tdb_set_attribute(tdb, &seed_attr) == TDB_ERR_EINVAL);
		ok1(tap_log_messages == 1);
		ok1(tdb_set_attribute(tdb, &hash_attr) == TDB_ERR_EINVAL);
		ok1(tap_log_messages == 2);
		tap_log_messages = 0;

		/* Getting them should work as expected. */
		attr.base.attr = TDB_ATTRIBUTE_LOG;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_LOG);
		ok1(attr.log.fn == tap_log_attr.log.fn);
		ok1(attr.log.data == tap_log_attr.log.data);

		attr.base.attr = TDB_ATTRIBUTE_FLOCK;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == mylock);
		ok1(attr.flock.unlock == myunlock);
		ok1(attr.flock.data == &lock_attr);

		/* Unset them again. */
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_FLOCK);
		ok1(tap_log_messages == 0);
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_LOG);
		ok1(tap_log_messages == 0);

		tdb_close(tdb);
		ok1(tap_log_messages == 0);

		/* Now open with all attributes. */
		tdb = tdb_open("run-90-get-set-attributes.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hash_attr);
		ok1(tdb);

		/* Get will succeed */
		attr.base.attr = TDB_ATTRIBUTE_LOG;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_LOG);
		ok1(attr.log.fn == tap_log_attr.log.fn);
		ok1(attr.log.data == tap_log_attr.log.data);

		attr.base.attr = TDB_ATTRIBUTE_HASH;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_HASH);
		ok1(attr.hash.fn == hash_fn);
		ok1(attr.hash.data == &hash_attr);

		attr.base.attr = TDB_ATTRIBUTE_FLOCK;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == mylock);
		ok1(attr.flock.unlock == myunlock);
		ok1(attr.flock.data == &lock_attr);

		attr.base.attr = TDB_ATTRIBUTE_SEED;
		ok1(tdb_get_attribute(tdb, &attr) == 0);
		ok1(attr.base.attr == TDB_ATTRIBUTE_SEED);
		ok1(attr.seed.seed == seed_attr.seed.seed);

		/* Unset attributes. */
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_HASH);
		ok1(tap_log_messages == 1);
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_SEED);
		ok1(tap_log_messages == 2);
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_FLOCK);
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_LOG);
		ok1(tap_log_messages == 2);
		tap_log_messages = 0;

		tdb_close(tdb);

	}
	return exit_status();
}
