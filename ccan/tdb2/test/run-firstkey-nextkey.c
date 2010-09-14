#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tap/tap.h>
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

int main(int argc, char *argv[])
{
	unsigned int i, j;
	int num;
	struct trav_data td;
	TDB_DATA k, k2;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (NUM_RECORDS*4 + (NUM_RECORDS-1)*2 + 20) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-traverse.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		ok1(tdb_firstkey(tdb).dptr == NULL);
		ok1(tdb_error(tdb) == TDB_SUCCESS);

		/* One entry... */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 0;
		ok1(tdb_store(tdb, k, k, TDB_INSERT) == 0);
		k = tdb_firstkey(tdb);
		ok1(k.dsize == sizeof(num));
		ok1(memcmp(k.dptr, &num, sizeof(num)) == 0);
		k2 = tdb_nextkey(tdb, k);
		ok1(k2.dsize == 0 && k2.dptr == NULL);
		free(k.dptr);

		/* Two entries. */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 1;
		ok1(tdb_store(tdb, k, k, TDB_INSERT) == 0);
		k = tdb_firstkey(tdb);
		ok1(k.dsize == sizeof(num));
		memcpy(&num, k.dptr, sizeof(num));
		ok1(num == 0 || num == 1);
		k2 = tdb_nextkey(tdb, k);
		ok1(k2.dsize == sizeof(j));
		free(k.dptr);
		memcpy(&j, k2.dptr, sizeof(j));
		ok1(j == 0 || j == 1);
		ok1(j != num);
		k = tdb_nextkey(tdb, k2);
		ok1(k.dsize == 0 && k.dptr == NULL);
		free(k2.dptr);

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

		num = tdb_traverse_read(tdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(td.calls == NUM_RECORDS);

		/* Simple loop should match tdb_traverse_read */
		for (j = 0, k = tdb_firstkey(tdb); j < td.calls; j++) {
			int val;

			ok1(k.dsize == sizeof(val));
			memcpy(&val, k.dptr, k.dsize);
			ok1(td.records[j] == val);
			k2 = tdb_nextkey(tdb, k);
			free(k.dptr);
			k = k2;
		}

		/* But arbitary orderings should work too. */
		for (j = td.calls-1; j > 0; j--) {
			k.dptr = (unsigned char *)&td.records[j-1];
			k.dsize = sizeof(td.records[j-1]);
			k = tdb_nextkey(tdb, k);
			ok1(k.dsize == sizeof(td.records[j]));
			ok1(memcmp(k.dptr, &td.records[j], k.dsize) == 0);
			free(k.dptr);
		}

		/* Even delete should work. */
		for (j = 0, k = tdb_firstkey(tdb); k.dptr; j++) {
			ok1(k.dsize == 4);
			ok1(tdb_delete(tdb, k) == 0);
			k2 = tdb_nextkey(tdb, k);
			free(k.dptr);
			k = k2;
		}

		diag("delete using first/nextkey gave %u of %u records",
		     j, NUM_RECORDS);
		ok1(j == NUM_RECORDS);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
