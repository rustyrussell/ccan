#include "ntdb-source.h"
#include "tap-interface.h"
#include "logging.h"
#include "helprun-external-agent.h"

static int log_count = 0;

/* Normally we get a log when setting random seed. */
static void my_log_fn(struct ntdb_context *ntdb,
		      enum ntdb_log_level level,
		      enum NTDB_ERROR ecode,
		      const char *message, void *priv)
{
	log_count++;
}

static union ntdb_attribute log_attr = {
	.log = { .base = { .attr = NTDB_ATTRIBUTE_LOG },
		 .fn = my_log_fn }
};

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	union ntdb_attribute attr;
	int flags[] = { NTDB_INTERNAL, NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_INTERNAL|NTDB_CONVERT, NTDB_CONVERT,
			NTDB_NOMMAP|NTDB_CONVERT };

	attr.seed.base.attr = NTDB_ATTRIBUTE_SEED;
	attr.seed.base.next = &log_attr;
	attr.seed.seed = 42;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 4 + 4 * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct ntdb_header hdr;
		int fd;
		ntdb = ntdb_open("run-seed.ntdb", flags[i]|MAYBE_NOSYNC,
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &attr);
		ok1(ntdb);
		if (!ntdb)
			continue;
		ok1(ntdb_check(ntdb, NULL, NULL) == 0);
		ok1(ntdb->hash_seed == 42);
		ok1(log_count == 0);
		ntdb_close(ntdb);

		if (flags[i] & NTDB_INTERNAL)
			continue;

		fd = open("run-seed.ntdb", O_RDONLY);
		ok1(fd >= 0);
		ok1(read(fd, &hdr, sizeof(hdr)) == sizeof(hdr));
		if (flags[i] & NTDB_CONVERT)
			ok1(bswap_64(hdr.hash_seed) == 42);
		else
			ok1(hdr.hash_seed == 42);
		close(fd);
	}
	return exit_status();
}
