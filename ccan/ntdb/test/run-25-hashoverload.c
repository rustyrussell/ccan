#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "helprun-external-agent.h"

#define OVERLOAD 100

static uint32_t badhash(const void *key, size_t len, uint32_t seed, void *priv)
{
	return 0;
}

static int trav(struct ntdb_context *ntdb, NTDB_DATA key, NTDB_DATA dbuf, void *p)
{
	if (p)
		return ntdb_delete(ntdb, key);
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	NTDB_DATA key = { (unsigned char *)&j, sizeof(j) };
	NTDB_DATA dbuf = { (unsigned char *)&j, sizeof(j) };
	union ntdb_attribute hattr = { .hash = { .base = { NTDB_ATTRIBUTE_HASH },
						.fn = badhash } };
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT,
	};

	hattr.base.next = &tap_log_attr;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * (7 * OVERLOAD + 11) + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		NTDB_DATA d = { NULL, 0 }; /* Bogus GCC warning */

		ntdb = ntdb_open("run-25-hashoverload.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &hattr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		/* Overload a bucket. */
		for (j = 0; j < OVERLOAD; j++) {
			ok1(ntdb_store(ntdb, key, dbuf, NTDB_INSERT) == 0);
		}
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Check we can find them all. */
		for (j = 0; j < OVERLOAD; j++) {
			ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
			ok1(d.dsize == sizeof(j));
			ok1(d.dptr != NULL);
			ok1(d.dptr && memcmp(d.dptr, &j, d.dsize) == 0);
			free(d.dptr);
		}

		/* Traverse through them. */
		ok1(ntdb_traverse(ntdb, trav, NULL) == OVERLOAD);

		/* Delete the first 99. */
		for (j = 0; j < OVERLOAD-1; j++)
			ok1(ntdb_delete(ntdb, key) == 0);

		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_SUCCESS);
		ok1(d.dsize == sizeof(j));
		ok1(d.dptr != NULL);
		ok1(d.dptr && memcmp(d.dptr, &j, d.dsize) == 0);
		free(d.dptr);

		/* Traverse through them. */
		ok1(ntdb_traverse(ntdb, trav, NULL) == 1);

		/* Re-add */
		for (j = 0; j < OVERLOAD-1; j++) {
			ok1(ntdb_store(ntdb, key, dbuf, NTDB_INSERT) == 0);
		}
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);

		/* Now try deleting as we go. */
		ok1(ntdb_traverse(ntdb, trav, trav) == OVERLOAD);
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb_traverse(ntdb, trav, NULL) == 0);
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
