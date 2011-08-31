#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <ccan/hash/hash.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logging.h"

/* We use the same seed which we saw a failure on. */
static uint64_t fixedhash(const void *key, size_t len, uint64_t seed, void *p)
{
	return hash64_stable((const unsigned char *)key, len,
			     *(uint64_t *)p);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct tdb_context *tdb;
	uint64_t seed = 16014841315512641303ULL;
	union tdb_attribute fixed_hattr
		= { .hash = { .base = { TDB_ATTRIBUTE_HASH },
			      .fn = fixedhash,
			      .data = &seed } };
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT,
			TDB_NOMMAP|TDB_CONVERT };
	struct tdb_data key = { (unsigned char *)&j, sizeof(j) };
	struct tdb_data data = { (unsigned char *)&j, sizeof(j) };

	fixed_hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (1 + 500 * 3) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-12-store.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &fixed_hattr);
		ok1(tdb);
		if (!tdb)
			continue;

		/* We seemed to lose some keys.
		 * Insert and check they're in there! */
		for (j = 0; j < 500; j++) {
			struct tdb_data d = { NULL, 0 }; /* Bogus GCC warning */
			ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
			ok1(tdb_fetch(tdb, key, &d) == TDB_SUCCESS);
			ok1(tdb_deq(d, data));
			free(d.dptr);
		}
		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
