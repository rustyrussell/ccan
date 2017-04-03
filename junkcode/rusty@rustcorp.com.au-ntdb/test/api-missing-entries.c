/* Another test revealed that we lost an entry.  This reproduces it. */
#include "config.h"
#include "../ntdb.h"
#include "../private.h"
#include <ccan/hash/hash.h>
#include "tap-interface.h"
#include "logging.h"
#include "helpapi-external-agent.h"

#define NUM_RECORDS 1189

/* We use the same seed which we saw this failure on. */
static uint32_t failhash(const void *key, size_t len, uint32_t seed, void *p)
{
	return hash64_stable((const unsigned char *)key, len,
			     699537674708983027ULL);
}

int main(int argc, char *argv[])
{
	int i;
	struct ntdb_context *ntdb;
	NTDB_DATA key = { (unsigned char *)&i, sizeof(i) };
	NTDB_DATA data = { (unsigned char *)&i, sizeof(i) };
	union ntdb_attribute hattr = { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
						.fn = failhash } };

	hattr.base.next = &tap_log_attr;
	plan_tests(1 + NUM_RECORDS + 2);

	ntdb = ntdb_open("run-missing-entries.ntdb", NTDB_INTERNAL,
			 O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
	if (ok1(ntdb)) {
		for (i = 0; i < NUM_RECORDS; i++) {
			ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
		}
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
