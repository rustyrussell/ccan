#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>
#include "logging.h"

int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	unsigned int i;
	struct tdb1_header hdr;
	struct tdb_data key = { (unsigned char *)&hdr, sizeof(hdr) };
	struct tdb_data data = { (unsigned char *)&hdr, sizeof(hdr) };
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 7);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-tdb1-seqnum-wrap.tdb1",
			       flags[i]|TDB_VERSION1|TDB_SEQNUM,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);
		if (!tdb)
			break;
		ok1(pread(tdb->file->fd, &hdr, sizeof(hdr), 0) == sizeof(hdr));
		hdr.sequence_number = 0xFFFFFFFF;
		ok1(pwrite(tdb->file->fd, &hdr, sizeof(hdr), 0) == sizeof(hdr));

		/* Must not be negative: that would mean an error! */
		ok1(tdb_get_seqnum(tdb) == 0xFFFFFFFF);

		ok1(tdb_store(tdb, key, data, TDB_INSERT) == TDB_SUCCESS);
		ok1(tdb_get_seqnum(tdb) == 0);
		tdb_close(tdb);
		ok1(tap_log_messages == 0);
	}

	return exit_status();
}
