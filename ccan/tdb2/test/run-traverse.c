#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include "logging.h"

#define NUM_RECORDS 1000

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
	struct tdb_data data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_RECORDS; i++)
		if (tdb_store(tdb, key, data, TDB_REPLACE) != 0)
			return false;
	return true;
}

struct trav_data {
	unsigned int calls, call_limit;
	int low, high;
	bool mismatch;
	bool delete;
	enum TDB_ERROR delete_error;
};

static int trav(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
		struct trav_data *td)
{
	int val;

	td->calls++;
	if (key.dsize != sizeof(val) || dbuf.dsize != sizeof(val)
	    || memcmp(key.dptr, dbuf.dptr, key.dsize) != 0) {
		td->mismatch = true;
		return -1;
	}
	memcpy(&val, dbuf.dptr, dbuf.dsize);
	if (val < td->low)
		td->low = val;
	if (val > td->high)
		td->high = val;

	if (td->delete) {
		td->delete_error = tdb_delete(tdb, key);
		if (td->delete_error != TDB_SUCCESS) {
			return -1;
		}
	}

	if (td->calls == td->call_limit)
		return 1;
	return 0;
}

struct trav_grow_data {
	unsigned int calls;
	unsigned int num_large;
	bool mismatch;
	enum TDB_ERROR error;
};

static int trav_grow(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf,
		     struct trav_grow_data *tgd)	     
{
	int val;
	unsigned char buffer[128] = { 0 };

	tgd->calls++;
	if (key.dsize != sizeof(val) || dbuf.dsize < sizeof(val)
	    || memcmp(key.dptr, dbuf.dptr, key.dsize) != 0) {
		tgd->mismatch = true;
		return -1;
	}

	if (dbuf.dsize > sizeof(val))
		/* We must have seen this before! */
		tgd->num_large++;

	/* Make a big difference to the database. */
	dbuf.dptr = buffer;
	dbuf.dsize = sizeof(buffer);
	tgd->error = tdb_append(tdb, key, dbuf);
	if (tgd->error != TDB_SUCCESS) {
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	int num;
	struct trav_data td;
	struct trav_grow_data tgd;
	struct tdb_context *tdb;
	uint64_t seed = 16014841315512641303ULL;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.fn = fixedhash,
						.data = &seed } };

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 32 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-traverse.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_traverse(tdb, NULL, NULL) == 0);

		ok1(store_records(tdb));
		num = tdb_traverse(tdb, NULL, NULL);
		ok1(num == NUM_RECORDS);

		/* Full traverse. */
		td.calls = 0;
		td.call_limit = UINT_MAX;
		td.low = INT_MAX;
		td.high = INT_MIN;
		td.mismatch = false;
		td.delete = false;

		num = tdb_traverse(tdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(!td.mismatch);
		ok1(td.calls == NUM_RECORDS);
		ok1(td.low == 0);
		ok1(td.high == NUM_RECORDS-1);

		/* Short traverse. */
		td.calls = 0;
		td.call_limit = NUM_RECORDS / 2;
		td.low = INT_MAX;
		td.high = INT_MIN;
		td.mismatch = false;
		td.delete = false;

		num = tdb_traverse(tdb, trav, &td);
		ok1(num == NUM_RECORDS / 2);
		ok1(!td.mismatch);
		ok1(td.calls == NUM_RECORDS / 2);
		ok1(td.low <= NUM_RECORDS / 2);
		ok1(td.high > NUM_RECORDS / 2);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tap_log_messages == 0);

		/* Deleting traverse (delete everything). */
		td.calls = 0;
		td.call_limit = UINT_MAX;
		td.low = INT_MAX;
		td.high = INT_MIN;
		td.mismatch = false;
		td.delete = true;
		td.delete_error = TDB_SUCCESS;
		num = tdb_traverse(tdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(td.delete_error == TDB_SUCCESS);
		ok1(!td.mismatch);
		ok1(td.calls == NUM_RECORDS);
		ok1(td.low == 0);
		ok1(td.high == NUM_RECORDS - 1);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Now it's empty! */
		ok1(tdb_traverse(tdb, NULL, NULL) == 0);

		/* Re-add. */
		ok1(store_records(tdb));
		ok1(tdb_traverse(tdb, NULL, NULL) == NUM_RECORDS);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Grow.  This will cause us to be reshuffled. */
		tgd.calls = 0;
		tgd.num_large = 0;
		tgd.mismatch = false;
		tgd.error = TDB_SUCCESS;
		ok1(tdb_traverse(tdb, trav_grow, &tgd) > 1);
		ok1(tgd.error == 0);
		ok1(!tgd.mismatch);
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tgd.num_large < tgd.calls);
		diag("growing db: %u calls, %u repeats",
		     tgd.calls, tgd.num_large);

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
