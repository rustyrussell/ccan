#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "logging.h"

#define NUM_RECORDS 1000

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
	unsigned int records[NUM_RECORDS];
	unsigned int calls;
};

static int trav(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf, void *p)
{
	struct trav_data *td = p;
	int val;

	memcpy(&val, dbuf.dptr, dbuf.dsize);
	td->records[td->calls++] = val;
	return 0;
}

/* Since tdb_nextkey frees dptr, we need to clone it. */
static TDB_DATA dup_key(TDB_DATA key)
{
	void *p = malloc(key.dsize);
	memcpy(p, key.dptr, key.dsize);
	key.dptr = p;
	return key;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	int num;
	struct trav_data td;
	TDB_DATA k;
	struct tdb_context *tdb;
	union tdb_attribute seed_attr;
	enum TDB_ERROR ecode;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
			TDB_INTERNAL|TDB_VERSION1, TDB_VERSION1,
			TDB_NOMMAP|TDB_VERSION1,
			TDB_INTERNAL|TDB_CONVERT|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };

	seed_attr.base.attr = TDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &tap_log_attr;
	seed_attr.seed.seed = 6334326220117065685ULL;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (NUM_RECORDS*6 + (NUM_RECORDS-1)*3 + 22) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-traverse.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600,
			       flags[i] & TDB_VERSION1 ? NULL : &seed_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_firstkey(tdb, &k) == TDB_ERR_NOEXIST);

		/* One entry... */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 0;
		ok1(tdb_store(tdb, k, k, TDB_INSERT) == 0);
		ok1(tdb_firstkey(tdb, &k) == TDB_SUCCESS);
		ok1(k.dsize == sizeof(num));
		ok1(memcmp(k.dptr, &num, sizeof(num)) == 0);
		ok1(tdb_nextkey(tdb, &k) == TDB_ERR_NOEXIST);

		/* Two entries. */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 1;
		ok1(tdb_store(tdb, k, k, TDB_INSERT) == 0);
		ok1(tdb_firstkey(tdb, &k) == TDB_SUCCESS);
		ok1(k.dsize == sizeof(num));
		memcpy(&num, k.dptr, sizeof(num));
		ok1(num == 0 || num == 1);
		ok1(tdb_nextkey(tdb, &k) == TDB_SUCCESS);
		ok1(k.dsize == sizeof(j));
		memcpy(&j, k.dptr, sizeof(j));
		ok1(j == 0 || j == 1);
		ok1(j != num);
		ok1(tdb_nextkey(tdb, &k) == TDB_ERR_NOEXIST);

		/* Clean up. */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 0;
		ok1(tdb_delete(tdb, k) == 0);
		num = 1;
		ok1(tdb_delete(tdb, k) == 0);

		/* Now lots of records. */
		ok1(store_records(tdb));
		td.calls = 0;

		num = tdb_traverse(tdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(td.calls == NUM_RECORDS);

		/* Simple loop should match tdb_traverse */
		for (j = 0, ecode = tdb_firstkey(tdb, &k); j < td.calls; j++) {
			int val;

			ok1(ecode == TDB_SUCCESS);
			ok1(k.dsize == sizeof(val));
			memcpy(&val, k.dptr, k.dsize);
			ok1(td.records[j] == val);
			ecode = tdb_nextkey(tdb, &k);
		}

		/* But arbitrary orderings should work too. */
		for (j = td.calls-1; j > 0; j--) {
			k.dptr = (unsigned char *)&td.records[j-1];
			k.dsize = sizeof(td.records[j-1]);
			k = dup_key(k);
			ok1(tdb_nextkey(tdb, &k) == TDB_SUCCESS);
			ok1(k.dsize == sizeof(td.records[j]));
			ok1(memcmp(k.dptr, &td.records[j], k.dsize) == 0);
			free(k.dptr);
		}

		/* Even delete should work. */
		for (j = 0, ecode = tdb_firstkey(tdb, &k);
		     ecode != TDB_ERR_NOEXIST;
		     j++) {
			ok1(ecode == TDB_SUCCESS);
			ok1(k.dsize == 4);
			ok1(tdb_delete(tdb, k) == 0);
			ecode = tdb_nextkey(tdb, &k);
		}

		diag("delete using first/nextkey gave %u of %u records",
		     j, NUM_RECORDS);
		ok1(j == NUM_RECORDS);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
