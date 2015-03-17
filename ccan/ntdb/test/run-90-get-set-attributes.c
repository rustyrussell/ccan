#include "ntdb-source.h"
#include "tap-interface.h"
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

static uint32_t hash_fn(const void *key, size_t len, uint32_t seed,
			void *priv)
{
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	union ntdb_attribute seed_attr;
	union ntdb_attribute hash_attr;
	union ntdb_attribute lock_attr;

	seed_attr.base.attr = NTDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &hash_attr;
	seed_attr.seed.seed = 100;

	hash_attr.base.attr = NTDB_ATTRIBUTE_HASH;
	hash_attr.base.next = &lock_attr;
	hash_attr.hash.fn = hash_fn;
	hash_attr.hash.data = &hash_attr;

	lock_attr.base.attr = NTDB_ATTRIBUTE_FLOCK;
	lock_attr.base.next = &tap_log_attr;
	lock_attr.flock.lock = mylock;
	lock_attr.flock.unlock = myunlock;
	lock_attr.flock.data = &lock_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 50);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		union ntdb_attribute attr;

		/* First open with no attributes. */
		ntdb = ntdb_open("run-90-get-set-attributes.ntdb",
				 flags[i] |MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, NULL);
		ok1(ntdb);

		/* Get log on no attributes will fail */
		attr.base.attr = NTDB_ATTRIBUTE_LOG;
		ok1(ntdb_get_attribute(ntdb, &attr) == NTDB_ERR_NOEXIST);
		/* These always work. */
		attr.base.attr = NTDB_ATTRIBUTE_HASH;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_HASH);
		ok1(attr.hash.fn == ntdb_jenkins_hash);
		attr.base.attr = NTDB_ATTRIBUTE_FLOCK;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == ntdb_fcntl_lock);
		ok1(attr.flock.unlock == ntdb_fcntl_unlock);
		attr.base.attr = NTDB_ATTRIBUTE_SEED;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_SEED);
		/* This is possible, just astronomically unlikely. */
		ok1(attr.seed.seed != 0);

		/* Unset attributes. */
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_LOG);
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_FLOCK);

		/* Set them. */
		ok1(ntdb_set_attribute(ntdb, &tap_log_attr) == 0);
		ok1(ntdb_set_attribute(ntdb, &lock_attr) == 0);
		/* These should fail. */
		ok1(ntdb_set_attribute(ntdb, &seed_attr) == NTDB_ERR_EINVAL);
		ok1(tap_log_messages == 1);
		ok1(ntdb_set_attribute(ntdb, &hash_attr) == NTDB_ERR_EINVAL);
		ok1(tap_log_messages == 2);
		tap_log_messages = 0;

		/* Getting them should work as expected. */
		attr.base.attr = NTDB_ATTRIBUTE_LOG;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_LOG);
		ok1(attr.log.fn == tap_log_attr.log.fn);
		ok1(attr.log.data == tap_log_attr.log.data);

		attr.base.attr = NTDB_ATTRIBUTE_FLOCK;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == mylock);
		ok1(attr.flock.unlock == myunlock);
		ok1(attr.flock.data == &lock_attr);

		/* Unset them again. */
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_FLOCK);
		ok1(tap_log_messages == 0);
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_LOG);
		ok1(tap_log_messages == 0);

		ntdb_close(ntdb);
		ok1(tap_log_messages == 0);

		/* Now open with all attributes. */
		ntdb = ntdb_open("run-90-get-set-attributes.ntdb",
				 flags[i] | MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600,
				 &seed_attr);

		ok1(ntdb);

		/* Get will succeed */
		attr.base.attr = NTDB_ATTRIBUTE_LOG;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_LOG);
		ok1(attr.log.fn == tap_log_attr.log.fn);
		ok1(attr.log.data == tap_log_attr.log.data);

		attr.base.attr = NTDB_ATTRIBUTE_HASH;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_HASH);
		ok1(attr.hash.fn == hash_fn);
		ok1(attr.hash.data == &hash_attr);

		attr.base.attr = NTDB_ATTRIBUTE_FLOCK;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_FLOCK);
		ok1(attr.flock.lock == mylock);
		ok1(attr.flock.unlock == myunlock);
		ok1(attr.flock.data == &lock_attr);

		attr.base.attr = NTDB_ATTRIBUTE_SEED;
		ok1(ntdb_get_attribute(ntdb, &attr) == 0);
		ok1(attr.base.attr == NTDB_ATTRIBUTE_SEED);
		ok1(attr.seed.seed == seed_attr.seed.seed);

		/* Unset attributes. */
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_HASH);
		ok1(tap_log_messages == 1);
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_SEED);
		ok1(tap_log_messages == 2);
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_FLOCK);
		ntdb_unset_attribute(ntdb, NTDB_ATTRIBUTE_LOG);
		ok1(tap_log_messages == 2);
		tap_log_messages = 0;

		ntdb_close(ntdb);

	}
	return exit_status();
}
