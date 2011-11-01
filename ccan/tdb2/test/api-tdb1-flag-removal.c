#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <ccan/hash/hash.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "logging.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3 + 1);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-12-store.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			continue;

		tdb_close(tdb);

		tdb = tdb_open("run-12-store.tdb", flags[i] | TDB_VERSION1,
			       O_RDWR, 0600, &tap_log_attr);
		if (!ok1(tdb))
			continue;
		/* It's not a version1 */
		ok1(!(tdb_get_flags(tdb) & TDB_VERSION1));

		tdb_close(tdb);
	}

	ok1(tap_log_messages == 0);
	return exit_status();
}
