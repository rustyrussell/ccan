#include "ntdb-source.h"
/* We had a bug where we marked the ntdb read-only for a ntdb_traverse_read.
 * If we then expanded the ntdb, we would remap read-only, and later SEGV. */
#include "tap-interface.h"
#include "external-agent.h"
#include "logging.h"
#include "helprun-external-agent.h"

static bool file_larger(int fd, ntdb_len_t size)
{
	struct stat st;

	fstat(fd, &st);
	return st.st_size != size;
}

static unsigned add_records_to_grow(struct agent *agent, int fd, ntdb_len_t size)
{
	unsigned int i;

	for (i = 0; !file_larger(fd, size); i++) {
		char data[50];
		sprintf(data, "%i=%i", i, i);
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
	struct ntdb_context *ntdb;
	NTDB_DATA d = ntdb_mkdata("hello", 5);
	const char filename[] = "run-remap-in-read_traverse.ntdb";

	plan_tests(4);

	agent = prepare_external_agent();

	ntdb = ntdb_open(filename, MAYBE_NOSYNC,
		       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);

	ok1(external_agent_operation(agent, OPEN, filename) == SUCCESS);
	i = add_records_to_grow(agent, ntdb->file->fd, ntdb->file->map_size);

	/* Do a traverse. */
	ok1(ntdb_traverse(ntdb, NULL, NULL) == i);

	/* Now store something! */
	ok1(ntdb_store(ntdb, d, d, NTDB_INSERT) == 0);
	ok1(tap_log_messages == 0);
	ntdb_close(ntdb);
	free_external_agent(agent);
	return exit_status();
}
