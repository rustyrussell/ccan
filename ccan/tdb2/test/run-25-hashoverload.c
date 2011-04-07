#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

static uint64_t badhash(const void *key, size_t len, uint64_t seed, void *priv)
{
	return 0;
}

static int trav(struct tdb_context *tdb, TDB_DATA key, TDB_DATA dbuf, void *p)
{
	if (p)
		return tdb_delete(tdb, key);
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	struct tdb_data key = { (unsigned char *)&j, sizeof(j) };
	struct tdb_data dbuf = { (unsigned char *)&j, sizeof(j) };
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.fn = badhash } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT,
	};

	hattr.base.next = &tap_log_attr;

	plan_tests(6883);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct tdb_data d = { NULL, 0 }; /* Bogus GCC warning */

		tdb = tdb_open("run-25-hashoverload.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* Fill a group. */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++) {
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
		}
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Now store one last value: should form chain. */
		ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Check we can find them all. */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS) + 1; j++) {
			ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
			ok1(d.dsize == sizeof(j));
			ok1(d.dptr != NULL);
			ok1(d.dptr && memcmp(d.dptr, &j, d.dsize) == 0);
			free(d.dptr);
		}

		/* Now add a *lot* more. */
		for (j = (1 << TDB_HASH_GROUP_BITS) + 1;
		     j < (16 << TDB_HASH_GROUP_BITS);
		     j++) {
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
			ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
			ok1(d.dsize == sizeof(j));
			ok1(d.dptr != NULL);
			ok1(d.dptr && memcmp(d.dptr, &j, d.dsize) == 0);
			free(d.dptr);
		}
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Traverse through them. */
		ok1(tdb_traverse(tdb, trav, NULL) == j);

		/* Empty the first chain-worth. */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++)
			ok1(tdb_delete(tdb, key) == 0);

		ok1(tdb_check(tdb, NULL, NULL) == 0);

		for (j = (1 << TDB_HASH_GROUP_BITS);
		     j < (16 << TDB_HASH_GROUP_BITS);
		     j++) {
			ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
			ok1(d.dsize == sizeof(j));
			ok1(d.dptr != NULL);
			ok1(d.dptr && memcmp(d.dptr, &j, d.dsize) == 0);
			free(d.dptr);
		}

		/* Traverse through them. */
		ok1(tdb_traverse(tdb, trav, NULL)
		    == (15 << TDB_HASH_GROUP_BITS));

		/* Re-add */
		for (j = 0; j < (1 << TDB_HASH_GROUP_BITS); j++) {
			ok1(tdb_store(tdb, key, dbuf, TDB_INSERT) == 0);
		}
		ok1(tdb_check(tdb, NULL, NULL) == 0);

		/* Now try deleting as we go. */
		ok1(tdb_traverse(tdb, trav, trav)
		    == (16 << TDB_HASH_GROUP_BITS));
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb_traverse(tdb, trav, NULL) == 0);
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
