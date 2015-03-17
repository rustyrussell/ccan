#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"

#define NUM_RECORDS 1000

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
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };

	for (i = 0; i < NUM_RECORDS; i++)
		if (ntdb_store(ntdb, key, data, NTDB_REPLACE) != 0)
			return false;
	return true;
}

struct trav_data {
	unsigned int calls, call_limit;
	int low, high;
	bool mismatch;
	bool delete;
	enum NTDB_ERROR delete_error;
};

static int trav(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf,
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
		td->delete_error = ntdb_delete(ntdb, key);
		if (td->delete_error != NTDB_SUCCESS) {
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
	enum NTDB_ERROR error;
};

static int trav_grow(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf,
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
	tgd->error = ntdb_append(ntdb, key, dbuf);
	if (tgd->error != NTDB_SUCCESS) {
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
	struct ntdb_context *ntdb;
	uint64_t seed = 16014841315512641303ULL;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	union ntdb_attribute hattr = { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
						.fn = fixedhash,
						.data = &seed } };

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 32 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-traverse.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_traverse(ntdb, NULL, NULL) == 0);

		ok1(store_records(ntdb));
		num = ntdb_traverse(ntdb, NULL, NULL);
		ok1(num == NUM_RECORDS);

		/* Full traverse. */
		td.calls = 0;
		td.call_limit = UINT_MAX;
		td.low = INT_MAX;
		td.high = INT_MIN;
		td.mismatch = false;
		td.delete = false;

		num = ntdb_traverse(ntdb, trav, &td);
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

		num = ntdb_traverse(ntdb, trav, &td);
		ok1(num == NUM_RECORDS / 2);
		ok1(!td.mismatch);
		ok1(td.calls == NUM_RECORDS / 2);
		ok1(td.low <= NUM_RECORDS / 2);
		ok1(td.high > NUM_RECORDS / 2);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(tap_log_messages == 0);

		/* Deleting traverse (delete everything). */
		td.calls = 0;
		td.call_limit = UINT_MAX;
		td.low = INT_MAX;
		td.high = INT_MIN;
		td.mismatch = false;
		td.delete = true;
		td.delete_error = NTDB_SUCCESS;
		num = ntdb_traverse(ntdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(td.delete_error == NTDB_SUCCESS);
		ok1(!td.mismatch);
		ok1(td.calls == NUM_RECORDS);
		ok1(td.low == 0);
		ok1(td.high == NUM_RECORDS - 1);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Now it's empty! */
		ok1(ntdb_traverse(ntdb, NULL, NULL) == 0);

		/* Re-add. */
		ok1(store_records(ntdb));
		ok1(ntdb_traverse(ntdb, NULL, NULL) == NUM_RECORDS);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Grow.  This will cause us to be reshuffled. */
		tgd.calls = 0;
		tgd.num_large = 0;
		tgd.mismatch = false;
		tgd.error = NTDB_SUCCESS;
		ok1(ntdb_traverse(ntdb, trav_grow, &tgd) > 1);
		ok1(tgd.error == 0);
		ok1(!tgd.mismatch);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(tgd.num_large < tgd.calls);
		diag("growing db: %u calls, %u repeats",
		     tgd.calls, tgd.num_large);

		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
