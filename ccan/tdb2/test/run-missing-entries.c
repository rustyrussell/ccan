/* Another test revealed that we lost an entry.  This reproduces it. */
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "logging.h"

#define NUM_RECORDS 1189

/* We use the same seed which we saw this failure on. */
static uint64_t failhash(const void *key, size_t len, uint64_t seed, void *p)
{
	seed = 699537674708983027ULL;
	return hash64_stable((const unsigned char *)key, len, seed);
}

int main(int argc, char *argv[])
{
	int i;
	struct tdb_context *tdb;
	struct tdb_data key = { (unsigned char *)&i, sizeof(i) };
	struct tdb_data data = { (unsigned char *)&i, sizeof(i) };
	union tdb_attribute hattr = { .hash = { .base = { TDB_ATTRIBUTE_HASH },
						.fn = failhash } };

	hattr.base.next = &tap_log_attr;
	plan_tests(1 + 2 * NUM_RECORDS + 1);

	tdb = tdb_open("run-missing-entries.tdb", TDB_INTERNAL,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
	ok1(tdb);
	if (tdb) {
		for (i = 0; i < NUM_RECORDS; i++) {
			ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
			ok1(tdb_check(tdb, NULL, NULL) == 0);
		}
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
