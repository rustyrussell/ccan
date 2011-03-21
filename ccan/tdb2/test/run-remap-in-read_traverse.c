/* We had a bug where we marked the tdb read-only for a tdb_traverse_read.
 * If we then expanded the tdb, we would remap read-only, and later SEGV. */
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include "external-agent.h"
#include "logging.h"

static bool file_larger(int fd, tdb_len_t size)
{
	struct stat st;

	fstat(fd, &st);
	return st.st_size != size;
}

static unsigned add_records_to_grow(struct agent *agent, int fd, tdb_len_t size)
{
	unsigned int i;

	for (i = 0; !file_larger(fd, size); i++) {
		char data[20];
		sprintf(data, "%i", i);
		if (external_agent_operation(agent, STORE, data) != SUCCESS)
			return 0;
	}
	diag("Added %u records to grow file", i);
	return i;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct agent *agent;
	struct tdb_context *tdb;
	struct tdb_data d = tdb_mkdata("hello", 5);
	const char filename[] = "run-remap-in-read_traverse.tdb";

	plan_tests(4);

	agent = prepare_external_agent();

	tdb = tdb_open(filename, TDB_DEFAULT,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);

	ok1(external_agent_operation(agent, OPEN, filename) == SUCCESS);
	i = add_records_to_grow(agent, tdb->file->fd, tdb->file->map_size);

	/* Do a traverse. */
	ok1(tdb_traverse(tdb, NULL, NULL) == i);

	/* Now store something! */
	ok1(tdb_store(tdb, d, d, TDB_INSERT) == 0);
	ok1(tap_log_messages == 0);
	tdb_close(tdb);
	free_external_agent(agent);
	return exit_status();
}
