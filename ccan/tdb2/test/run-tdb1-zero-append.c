#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(4);
	tdb = tdb1_open(NULL, 1024, TDB_INTERNAL, O_CREAT|O_TRUNC|O_RDWR,
			0600, &tap_log_attr);
	ok1(tdb);

	/* Tickle bug on appending zero length buffer to zero length buffer. */
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dptr = (void *)"world";
	data.dsize = 0;

	ok1(tdb1_append(tdb, key, data) == 0);
	ok1(tdb1_append(tdb, key, data) == 0);
	data = tdb1_fetch(tdb, key);
	ok1(data.dsize == 0);
	free(data.dptr);
	tdb1_close(tdb);

	return exit_status();
}
