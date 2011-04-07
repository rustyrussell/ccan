#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

/* We rig the hash so adjacent-numbered records always clash. */
static uint64_t clash(const void *key, size_t len, uint64_t seed, void *priv)
{
	return ((uint64_t)*(const unsigned int *)key)
		<< (64 - TDB_TOPLEVEL_HASH_BITS - 1);
}

/* We use the same seed which we saw a failure on. */
static uint64_t fixedhash(const void *key, size_t len, uint64_t seed, void *p)
{
	return hash64_stable((const unsigned char *)key, len,
			     *(uint64_t *)p);
}

static bool store_records(struct tdb_context *tdb)
{
	int i;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data d, data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < 1000; i++) {
		if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
			return false;
		tdb_fetch(tdb, key, &d);
		if (!tdb_deq(d, data))
			return false;
		free(d.dptr);
	}
	return true;
}

static void test_val(struct tdb_context *tdb, uint64_t val)
{
	uint64_t v;
	struct tdb_data key = { (unsigned char *)&v, sizeof(v) };
	struct tdb_data d, data = { (unsigned char *)&v, sizeof(v) };

	/* Insert an entry, then delete it. */
	v = val;
	/* Delete should fail. */
	ok1(tdb_delete(tdb, key) == TDB_ERR_NOEXIST);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Insert should succeed. */
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Delete should succeed. */
	ok1(tdb_delete(tdb, key) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Re-add it, then add collision. */
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	v = val + 1;
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Can find both? */
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Delete second one. */
	v = val + 1;
	ok1(tdb_delete(tdb, key) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Re-add */
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Now, try deleting first one. */
	v = val;
	ok1(tdb_delete(tdb, key) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* Can still find second? */
	v = val + 1;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Now, this will be ideally placed. */
	v = val + 2;
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* This will collide with both. */
	v = val;
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);

	/* We can still find them all, right? */
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 1;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 2;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* And if we delete val + 1, that val + 2 should not move! */
	v = val + 1;
	ok1(tdb_delete(tdb, key) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	v = val;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 2;
	ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Delete those two, so we are empty. */
	ok1(tdb_delete(tdb, key) == 0);
	v = val;
	ok1(tdb_delete(tdb, key) == 0);

	ok1(tdb_check(tdb, NULL, NULL) == 0);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	uint64_t seed = 16014841315512641303ULL;
	union tdb_attribute clash_hattr
		= { .hash = { .base = { TDB_ATTRIBUTE_HASH },
			      .fn = clash } };
	union tdb_attribute fixed_hattr
		= { .hash = { .base = { TDB_ATTRIBUTE_HASH },
			      .fn = fixedhash,
			      .data = &seed } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };
	/* These two values gave trouble before. */
	int vals[] = { 755, 837 };

	clash_hattr.base.next = &tap_log_attr;
	fixed_hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (39 * 3 + 5 + sizeof(vals)/sizeof(vals[0])*2) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-13-delete.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &clash_hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Check start of hash table. */
		test_val(tdb, 0);

		/* Check end of hash table. */
		test_val(tdb, -1ULL);

		/* Check mixed bitpattern. */
		test_val(tdb, 0x123456789ABCDEF0ULL);

		ok1(!tdb->file || (tdb->file->allrecord_lock.count == 0
				   && tdb->file->num_lockrecs == 0));
		tdb_close(tdb);

		/* Deleting these entries in the db gave problems. */
		tdb = tdb_open("run-13-delete.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &fixed_hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(store_records(tdb));
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		for (j = 0; j < sizeof(vals)/sizeof(vals[0]); j++) {
			struct tdb_data key;

			key.dptr = (unsigned char *)&vals[j];
			key.dsize = sizeof(vals[j]);
			ok1(tdb_delete(tdb, key) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
		}
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
