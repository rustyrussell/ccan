#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include "tap-interface.h"
#include <ccan/hash/hash.h>
#include "logging.h"
#include "helpapi-external-agent.h"

/* We use the same seed which we saw a failure on. */
static uint32_t fixedhash(const void *key, size_t len, uint32_t seed, void *p)
{
	return hash64_stable((const unsigned char *)key, len,
			     *(uint64_t *)p);
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	uint64_t seed = 16014841315512641303ULL;
	union ntdb_attribute fixed_hattr
		= { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
			      .fn = fixedhash,
			      .data = &seed } };
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };
	NTDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	NTDB_DATA data = { (unsigned char *)&j, sizeof(j) };

	fixed_hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (1 + 500 * 3) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		ntdb = ntdb_open("run-12-store.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &fixed_hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* We seemed to lose some keys.
		 * Insert and check they're in there! */
		for (j = 0; j < 500; j++) {
			NTDB_DATA d = { NULL, 0 }; /* Bogus GCC warning */
			ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
			ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
			ok1(ntdb_deq(d, data));
			free(d.dptr);
		}
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
