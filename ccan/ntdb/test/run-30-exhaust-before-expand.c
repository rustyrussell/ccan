#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"

static bool empty_freetable(struct ntdb_context *ntdb)
{
	struct ntdb_freetable ftab;
	unsigned int i;

	/* Now, free table should be completely exhausted in zone 0 */
	if (ntdb_read_convert(ntdb, ntdb->ftable_off, &ftab, sizeof(ftab)) != 0)
		abort();

	for (i = 0; i < sizeof(ftab.buckets)/sizeof(ftab.buckets[0]); i++) {
		if (ftab.buckets[i])
			return false;
	}
	return true;
}


int main(int argc, char *argv[])
{
	unsigned int i, j;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7 + 1);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		NTDB_DATA k, d;
		uint64_t size;
		bool was_empty = false;

		k.dptr = (void *)&j;
		k.dsize = sizeof(j);

		ntdb = ntdb_open("run-30-exhaust-before-expand.ntdb",
				 flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);
		if (!ntdb)
			continue;

		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		/* There's one empty record in initial db. */
		ok1(!empty_freetable(ntdb));

		size = ntdb->file->map_size;

		/* Create one record to chew up most space. */
		d.dsize = size - NEW_DATABASE_HDR_SIZE(ntdb->hash_bits) - 32;
		d.dptr = calloc(d.dsize, 1);
		j = 0;
		ok1(ntdb_store(ntdb, k, d, NTDB_INSERT) == 0);
		ok1(ntdb->file->map_size == size);
		free(d.dptr);

		/* Now insert minimal-length records until we expand. */
		for (j = 1; ntdb->file->map_size == size; j++) {
			was_empty = empty_freetable(ntdb);
			if (ntdb_store(ntdb, k, k, NTDB_INSERT) != 0)
				err(1, "Failed to store record %i", j);
		}

		/* Would have been empty before expansion, but no longer. */
		ok1(was_empty);
		ok1(!empty_freetable(ntdb));
		ntdb_close(ntdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
