#include "../private.h" // For NTDB_TOPLEVEL_HASH_BITS
#include <ccan/hash/hash.h>
#include "../ntdb.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

/* We rig the hash so adjacent-numbered records always clash. */
static uint32_t clash(const void *key, size_t len, uint32_t seed, void *priv)
{
	return *((const unsigned int *)key) / 2;
}

/* We use the same seed which we saw a failure on. */
static uint32_t fixedhash(const void *key, size_t len, uint32_t seed, void *p)
{
	return hash64_stable((const unsigned char *)key, len,
			     *(uint64_t *)p);
}

static bool store_records(struct ntdb_context *ntdb)
{
	int i;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA d, data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < 1000; i++) {
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
			return false;
		ntdb_fetch(ntdb, key, &d);
		if (!ntdb_deq(d, data))
			return false;
		free(d.dptr);
	}
	return true;
}

static void test_val(struct ntdb_context *ntdb, uint64_t val)
{
	uint64_t v;
	NTDB_DATA key = { (unsigned char *)&v, sizeof(v) };
	NTDB_DATA d, data = { (unsigned char *)&v, sizeof(v) };

	/* Insert an entry, then delete it. */
	v = val;
	/* Delete should fail. */
	ok1(ntdb_delete(ntdb, key) == NTDB_ERR_NOEXIST);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Insert should succeed. */
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Delete should succeed. */
	ok1(ntdb_delete(ntdb, key) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Re-add it, then add collision. */
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
	v = val + 1;
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Can find both? */
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Delete second one. */
	v = val + 1;
	ok1(ntdb_delete(ntdb, key) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Re-add */
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Now, try deleting first one. */
	v = val;
	ok1(ntdb_delete(ntdb, key) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* Can still find second? */
	v = val + 1;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Now, this will be ideally placed. */
	v = val + 2;
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	/* This will collide with both. */
	v = val;
	ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);

	/* We can still find them all, right? */
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 1;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 2;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* And if we delete val + 1, that val + 2 should not move! */
	v = val + 1;
	ok1(ntdb_delete(ntdb, key) == 0);
	ok1(ntdb_check(ntdb, NULL, NULL) == 0);

	v = val;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);
	v = val + 2;
	ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
	ok1(d.dsize == data.dsize);
	free(d.dptr);

	/* Delete those two, so we are empty. */
	ok1(ntdb_delete(ntdb, key) == 0);
	v = val;
	ok1(ntdb_delete(ntdb, key) == 0);

	ok1(ntdb_check(ntdb, NULL, NULL) == 0);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	uint64_t seed = 16014841315512641303ULL;
	union ntdb_attribute clash_hattr
		= { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
			      .fn = clash } };
	union ntdb_attribute fixed_hattr
		= { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
			      .fn = fixedhash,
			      .data = &seed } };
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	/* These two values gave trouble before. */
	int vals[] = { 755, 837 };

	clash_hattr.base.next = &tap_log_attr;
	fixed_hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (39 * 3 + 5 + sizeof(vals)/sizeof(vals[0])*2) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-13-delete.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &clash_hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Check start of hash table. */
		test_val(ntdb, 0);

		/* Check end of hash table. */
		test_val(ntdb, -1ULL);

		/* Check mixed bitpattern. */
		test_val(ntdb, 0x123456789ABCDEF0ULL);

		ok1(!ntdb->file || (ntdb->file->allrecord_lock.count == 0
				   && ntdb->file->num_lockrecs == 0));
		ntdb_close(ntdb);

		/* Deleting these entries in the db gave problems. */
		ntdb = ntdb_open("run-13-delete.ntdb", flags[i]|MAYBE_NOSYNC,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &fixed_hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(store_records(ntdb));
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		for (j = 0; j < sizeof(vals)/sizeof(vals[0]); j++) {
			NTDB_DATA key;

			key.dptr = (unsigned char *)&vals[j];
			key.dsize = sizeof(vals[j]);
			ok1(ntdb_delete(ntdb, key) == 0);
			ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		}
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
