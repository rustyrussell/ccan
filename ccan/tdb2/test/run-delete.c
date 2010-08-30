#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

/* We rig the hash so adjacent-numbered records always clash. */
static uint64_t clash(const void *key, size_t len, uint64_t seed, void *priv)
{
	return *(unsigned int *)key / 2;
}

static void test_val(struct tdb_context *tdb, unsigned int val)
{
	unsigned int v;
	struct tdb_data key = { (unsigned char *)&v, sizeof(v) };
	struct tdb_data data = { (unsigned char *)&v, sizeof(v) };

	/* Insert an entry, then delete it. */
	v = val;
	/* Delete should fail. */
	ok1(tdb_delete(tdb, key) == -1);
	ok1(tdb_error(tdb) == TDB_ERR_NOEXIST);
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
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);
	v = val;
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);

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
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);

	/* Now, this will be ideally placed. */
	v = val + 2;
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	/* This will collide with both. */
	v = val;
	ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);

	/* We can still find them all, right? */
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);
	v = val + 1;
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);
	v = val + 2;
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);

	/* And if we delete val + 1, that val + 2 should not move! */
	v = val + 1;
	ok1(tdb_delete(tdb, key) == 0);
	ok1(tdb_check(tdb, NULL, NULL) == 0);

	v = val;
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);
	v = val + 2;
	ok1(tdb_fetch(tdb, key).dsize == data.dsize);

	/* Delete those two, so we are empty. */
	ok1(tdb_delete(tdb, key) == 0);
	v = val;
	ok1(tdb_delete(tdb, key) == 0);

	ok1(tdb_check(tdb, NULL, NULL) == 0);
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.hash_fn = clash } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 66 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-delete.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Check start of hash table. */
		test_val(tdb, 0);

		/* Check end of hash table (will wrap around!). */
		test_val(tdb, ((1 << tdb->header.v.hash_bits) - 1) * 2);

		ok1(!tdb_has_locks(tdb));
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
