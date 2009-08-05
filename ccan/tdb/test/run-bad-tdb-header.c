#define _XOPEN_SOURCE 500
#include "tdb/tdb.h"
#include "tdb/io.c"
#include "tdb/tdb.c"
#include "tdb/lock.c"
#include "tdb/freelist.c"
#include "tdb/traverse.c"
#include "tdb/transaction.c"
#include "tdb/error.c"
#include "tdb/open.c"
#include "tap/tap.h"
#include <stdlib.h>
#include <err.h>

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	struct tdb_header hdr;
	int fd;

	plan_tests(11);
	/* Can open fine if complete crap, as long as O_CREAT. */
	fd = open("/tmp/test.tdb", O_RDWR|O_CREAT|O_TRUNC, 0600);
	ok1(fd >= 0);
	ok1(write(fd, "hello world", 11) == 11);
	close(fd);
	tdb = tdb_open("/tmp/test.tdb", 1024, 0, O_RDWR, 0);
	ok1(!tdb);
	tdb = tdb_open("/tmp/test.tdb", 1024, 0, O_CREAT|O_RDWR, 0600);
	ok1(tdb);
	tdb_close(tdb);

	/* Now, with wrong version it should *not* overwrite. */
	fd = open("/tmp/test.tdb", O_RDWR);
	ok1(fd >= 0);
	ok1(read(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	ok1(hdr.version == TDB_VERSION);
	hdr.version++;
	lseek(fd, 0, SEEK_SET);
	ok1(write(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	close(fd);

	tdb = tdb_open("/tmp/test.tdb", 1024, 0, O_RDWR|O_CREAT, 0600);
	ok1(errno == EIO);
	ok1(!tdb);

	/* With truncate, will be fine. */
	tdb = tdb_open("/tmp/test.tdb", 1024, 0, O_RDWR|O_CREAT|O_TRUNC, 0600);
	ok1(tdb);
	tdb_close(tdb);
	unlink("/tmp/test.tdb");

	return exit_status();
}
