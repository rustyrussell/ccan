#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include "logging.h"

static int log_count = 0;

/* Normally we get a log when setting random seed. */
static void my_log_fn(struct tdb_context *tdb,
		      enum tdb_debug_level level, void *priv,
		      const char *fmt, ...)
{
	log_count++;
}

static union tdb_attribute log_attr = {
	.log = { .base = { .attr = TDB_ATTRIBUTE_LOG },
		 .log_fn = my_log_fn }
};

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	union tdb_attribute attr;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	attr.seed.base.attr = TDB_ATTRIBUTE_SEED;
	attr.seed.base.next = &log_attr;
	attr.seed.seed = 42;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4 + 4 * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct tdb_header hdr;
		int fd;
		tdb = tdb_open("run-seed.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &attr);
		ok1(tdb);
		if (!tdb)
			continue;
		ok1(tdb_check(tdb, NULL, NULL) == 0);
		ok1(tdb->hash_seed == 42);
		ok1(log_count == 0);
		tdb_close(tdb);

		if (flags[i] & TDB_INTERNAL)
			continue;

		fd = open("run-seed.tdb", O_RDONLY);
		ok1(fd >= 0);
		ok1(read(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
		if (flags[i] & TDB_CONVERT)
			ok1(bswap_64(hdr.hash_seed) == 42);
		else
			ok1(hdr.hash_seed == 42);
		close(fd);
	}
	return exit_status();
}
