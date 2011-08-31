#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static void log_fn(struct tdb1_context *tdb, enum tdb1_debug_level level, const char *fmt, ...)
{
	unsigned int *count = tdb1_get_logging_private(tdb);
	if (strstr(fmt, "spinlocks"))
		(*count)++;
}

/* The code should barf on TDBs created with rwlocks. */
int main(int argc, char *argv[])
{
	struct tdb1_context *tdb;
	unsigned int log_count;
	struct tdb1_logging_context log_ctx = { log_fn, &log_count };

	plan_tests(4);

	/* We should fail to open rwlock-using tdbs of either endian. */
	log_count = 0;
	tdb = tdb1_open_ex("test/rwlock-le.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb1_open_ex("test/rwlock-be.tdb1", 0, 0, O_RDWR, 0,
			  &log_ctx, NULL);
	ok1(!tdb);
	ok1(log_count == 1);

	return exit_status();
}
