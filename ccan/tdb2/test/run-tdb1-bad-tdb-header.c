#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "tdb1-logging.h"

int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	struct tdb1_header hdr;
	int fd;

	plan_tests(11);
	/* Can open fine if complete crap, as long as O_CREAT. */
	fd = open("run-bad-tdb-header.tdb", O_RDWR|O_CREAT|O_TRUNC, 0600);
	ok1(fd >= 0);
	ok1(write(fd, "hello world", 11) == 11);
	close(fd);
	tdb = tdb1_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_RDWR, 0,
			  &taplogctx, NULL);
	ok1(!tdb);
	tdb = tdb1_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_CREAT|O_RDWR,
			  0600, &taplogctx, NULL);
	ok1(tdb);
	tdb1_close(tdb);

	/* Now, with wrong version it should *not* overwrite. */
	fd = open("run-bad-tdb-header.tdb", O_RDWR);
	ok1(fd >= 0);
	ok1(read(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	ok1(hdr.version == TDB1_VERSION);
	hdr.version++;
	lseek(fd, 0, SEEK_SET);
	ok1(write(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
	close(fd);

	tdb = tdb1_open_ex("run-bad-tdb-header.tdb", 1024, 0, O_RDWR|O_CREAT,
			  0600, &taplogctx, NULL);
	ok1(errno == EIO);
	ok1(!tdb);

	/* With truncate, will be fine. */
	tdb = tdb1_open_ex("run-bad-tdb-header.tdb", 1024, 0,
			  O_RDWR|O_CREAT|O_TRUNC, 0600, &taplogctx, NULL);
	ok1(tdb);
	tdb1_close(tdb);

	return exit_status();
}
