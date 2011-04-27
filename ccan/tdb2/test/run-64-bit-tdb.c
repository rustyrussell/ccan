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

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	if (sizeof(off_t) <= 4) {
		plan_tests(1);
		pass("No 64 bit off_t");
		return exit_status();
	}

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 14);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		off_t old_size;
		TDB_DATA k, d;
		struct hash_info h;
		struct tdb_used_record rec;
		tdb_off_t off;

		tdb = tdb_open("run-64-bit-tdb.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			continue;

		old_size = tdb->file->map_size;

		/* This makes a sparse file */
		ok1(ftruncate(tdb->file->fd, 0xFFFFFFF0) == 0);
		ok1(add_free_record(tdb, old_size, 0xFFFFFFF0 - old_size,
				    TDB_LOCK_WAIT, false) == TDB_SUCCESS);

		/* Now add a little record past the 4G barrier. */
		ok1(tdb_expand_file(tdb, 100) == TDB_SUCCESS);
		ok1(add_free_record(tdb, 0xFFFFFFF0, 100, TDB_LOCK_WAIT, false)
		    == TDB_SUCCESS);

		ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);

		/* Test allocation path. */
		k = tdb_mkdata("key", 4);
		d = tdb_mkdata("data", 5);
		ok1(tdb_store(tdb, k, d, TDB_INSERT) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);

		/* Make sure it put it at end as we expected. */
		off = find_and_lock(tdb, k, F_RDLCK, &h, &rec, NULL);
		ok1(off >= 0xFFFFFFF0);
		tdb_unlock_hashes(tdb, h.hlock_start, h.hlock_range, F_RDLCK);

		ok1(tdb_fetch(tdb, k, &d) == 0);
		ok1(d.dsize == 5);
		ok1(strcmp((char *)d.dptr, "data") == 0);
		free(d.dptr);

		ok1(tdb_delete(tdb, k) == 0);
		ok1(tdb_check(tdb, NULL, NULL) == TDB_SUCCESS);

		tdb_close(tdb);
	}

	/* We might get messages about mmap failing, so don't test
	 * tap_log_messages */
	return exit_status();
}
