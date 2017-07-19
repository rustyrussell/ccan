#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

#define NUM_RECORDS 1000

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
	unsigned int records[NUM_RECORDS];
	unsigned int calls;
};

static int trav(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf, void *p)
{
	struct trav_data *td = p;
	int val;

	memcpy(&val, dbuf.dptr, dbuf.dsize);
	td->records[td->calls++] = val;
	return 0;
}

/* Since ntdb_nextkey frees dptr, we need to clone it. */
static NTDB_DATA dup_key(NTDB_DATA key)
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
	NTDB_DATA k;
	struct ntdb_context *ntdb;
	union ntdb_attribute seed_attr;
	enum NTDB_ERROR ecode;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	seed_attr.base.attr = NTDB_ATTRIBUTE_SEED;
	seed_attr.base.next = &tap_log_attr;
	seed_attr.seed.seed = 6334326220117065685ULL;

	plan_tests(sizeof(flags) / sizeof(flags[0])
		   * (NUM_RECORDS*6 + (NUM_RECORDS-1)*3 + 22) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("api-firstkey-nextkey.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600,
				 &seed_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_firstkey(ntdb, &k) == NTDB_ERR_NOEXIST);

		/* One entry... */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 0;
		ok1(ntdb_store(ntdb, k, k, NTDB_INSERT) == 0);
		ok1(ntdb_firstkey(ntdb, &k) == NTDB_SUCCESS);
		ok1(k.dsize == sizeof(num));
		ok1(memcmp(k.dptr, &num, sizeof(num)) == 0);
		ok1(ntdb_nextkey(ntdb, &k) == NTDB_ERR_NOEXIST);

		/* Two entries. */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 1;
		ok1(ntdb_store(ntdb, k, k, NTDB_INSERT) == 0);
		ok1(ntdb_firstkey(ntdb, &k) == NTDB_SUCCESS);
		ok1(k.dsize == sizeof(num));
		memcpy(&num, k.dptr, sizeof(num));
		ok1(num == 0 || num == 1);
		ok1(ntdb_nextkey(ntdb, &k) == NTDB_SUCCESS);
		ok1(k.dsize == sizeof(j));
		memcpy(&j, k.dptr, sizeof(j));
		ok1(j == 0 || j == 1);
		ok1(j != num);
		ok1(ntdb_nextkey(ntdb, &k) == NTDB_ERR_NOEXIST);

		/* Clean up. */
		k.dptr = (unsigned char *)&num;
		k.dsize = sizeof(num);
		num = 0;
		ok1(ntdb_delete(ntdb, k) == 0);
		num = 1;
		ok1(ntdb_delete(ntdb, k) == 0);

		/* Now lots of records. */
		ok1(store_records(ntdb));
		td.calls = 0;

		num = ntdb_traverse(ntdb, trav, &td);
		ok1(num == NUM_RECORDS);
		ok1(td.calls == NUM_RECORDS);

		/* Simple loop should match ntdb_traverse */
		for (j = 0, ecode = ntdb_firstkey(ntdb, &k); j < td.calls; j++) {
			int val;

			ok1(ecode == NTDB_SUCCESS);
			ok1(k.dsize == sizeof(val));
			memcpy(&val, k.dptr, k.dsize);
			ok1(td.records[j] == val);
			ecode = ntdb_nextkey(ntdb, &k);
		}

		/* But arbitrary orderings should work too. */
		for (j = td.calls-1; j > 0; j--) {
			k.dptr = (unsigned char *)&td.records[j-1];
			k.dsize = sizeof(td.records[j-1]);
			k = dup_key(k);
			ok1(ntdb_nextkey(ntdb, &k) == NTDB_SUCCESS);
			ok1(k.dsize == sizeof(td.records[j]));
			ok1(memcmp(k.dptr, &td.records[j], k.dsize) == 0);
			free(k.dptr);
		}

		/* Even delete should work. */
		for (j = 0, ecode = ntdb_firstkey(ntdb, &k);
		     ecode != NTDB_ERR_NOEXIST;
		     j++) {
			ok1(ecode == NTDB_SUCCESS);
			ok1(k.dsize == 4);
			ok1(ntdb_delete(ntdb, k) == 0);
			ecode = ntdb_nextkey(ntdb, &k);
		}

		diag("delete using first/nextkey gave %u of %u records",
		     j, NUM_RECORDS);
		ok1(j == NUM_RECORDS);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
