#define _XOPEN_SOURCE 500
#include <ccan/tdb/tdb.h>
#include <ccan/tdb/io.c>
#include <ccan/tdb/tdb.c>
#include <ccan/tdb/lock.c>
#include <ccan/tdb/freelist.c>
#include <ccan/tdb/traverse.c>
#include <ccan/tdb/transaction.c>
#include <ccan/tdb/error.c>
#include <ccan/tdb/open.c>
#include <ccan/tdb/check.c>
#include <ccan/tdb/hash.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	struct tdb_header hdr;
	int fd;

	plan_tests(11);
	/* Can open fine if complete crap, as long as O_CREAT. */
	fd = open("run-bad-tdb-header.tdb", O_RDWR|O_CREAT|O_TRUNC, 0600);
	ok1(fd >= 0);
	ok1(write(fd, "hello world", 11) == 11);
	close(fd);
	tdb = tdb_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(!tdb);
	tdb = tdb_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_CREAT|O_RDWR,
			  0600, &taplogctx, NULL);
	ok1(tdb);
	tdb_close(tdb);

	/* Now, with wrong version it should *not* overwrite. */
	fd = open("run-bad-tdb-header.tdb", O_RDWR);
	ok1(fd >= 0);
	ok1(read(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	ok1(hdr.version == TDB_VERSION);
	hdr.version++;
	lseek(fd, 0, SEEK_SET);
	ok1(write(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	close(fd);

	tdb = tdb_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_RDWR|O_CREAT,
			  0600, &taplogctx, NULL);
	ok1(errno == EIO);
	ok1(!tdb);

	/* With truncate, will be fine. */
	tdb = tdb_open_ex("run-bad-tdb-header.tdb", 1024, 0,
			  O_RDWR|O_CREAT|O_TRUNC, 0600, &taplogctx, NULL);
	ok1(tdb);
	tdb_close(tdb);

	return exit_status();
}
