#include "tdb2-source.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <err.h>

static void log_fn(struct tdb_context *tdb, enum tdb_log_level level,
		   enum TDB_ERROR ecode, const char *message, void *priv)
{
	unsigned int *count = priv;
	if (strstr(message, "spinlocks"))
		(*count)++;
}

/* The code should barf on TDBs created with rwlocks. */
int main(int argc, char *argv[])
{
	struct tdb_context *tdb;
	unsigned int log_count;
	union tdb_attribute log_attr;

	log_attr.base.attr = TDB_ATTRIBUTE_LOG;
	log_attr.base.next = NULL;
	log_attr.log.fn = log_fn;
	log_attr.log.data = &log_count;

	plan_tests(4);

	/* We should fail to open rwlock-using tdbs of either endian. */
	log_count = 0;
	tdb = tdb1_open("test/rwlock-le.tdb1", 0, O_RDWR, 0,
			&log_attr);
	ok1(!tdb);
	ok1(log_count == 1);

	log_count = 0;
	tdb = tdb1_open("test/rwlock-be.tdb1", 0, O_RDWR, 0,
			&log_attr);
	ok1(!tdb);
	ok1(log_count == 1);

	return exit_status();
}
