#include "../private.h"
#include <unistd.h>
#include "lock-tracking.h"

static ssize_t pwrite_check(int fd, const void *buf, size_t count, off_t offset);
static ssize_t write_check(int fd, const void *buf, size_t count);
static int ftruncate_check(int fd, off_t length);

#define pwrite pwrite_check
#define write write_check
#define fcntl fcntl_with_lockcheck
#define ftruncate ftruncate_check

#include "ntdb-source.h"
#include "tap-interface.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include "external-agent.h"
#include "logging.h"
#include "helprun-external-agent.h"

static struct agent *agent;
static bool opened;
static int errors = 0;
#define TEST_DBNAME "run-56-open-during-transaction.ntdb"

#undef write
#undef pwrite
#undef fcntl
#undef ftruncate

static bool is_same(const char *snapshot, const char *latest, off_t len)
{
	unsigned i;

	for (i = 0; i < len; i++) {
		if (snapshot[i] != latest[i])
			return false;
	}
	return true;
}

static bool compare_file(int fd, const char *snapshot, off_t snapshot_len)
{
	char *contents;
	bool ret;

	/* over-length read serves as length check. */
	contents = malloc(snapshot_len+1);
	ret = pread(fd, contents, snapshot_len+1, 0) == snapshot_len
		&& is_same(snapshot, contents, snapshot_len);
	free(contents);
	return ret;
}

static void check_file_intact(int fd)
{
	enum agent_return ret;
	struct stat st;
	char *contents;

	fstat(fd, &st);
	contents = malloc(st.st_size);
	if (pread(fd, contents, st.st_size, 0) != st.st_size) {
		diag("Read fail");
		errors++;
		return;
	}

	/* Ask agent to open file. */
	ret = external_agent_operation(agent, OPEN, TEST_DBNAME);

	/* It's OK to open it, but it must not have changed! */
	if (!compare_file(fd, contents, st.st_size)) {
		diag("Agent changed file after opening %s",
		     agent_return_name(ret));
		errors++;
	}

	if (ret == SUCCESS) {
		ret = external_agent_operation(agent, CLOSE, NULL);
		if (ret != SUCCESS) {
			diag("Agent failed to close ntdb: %s",
			     agent_return_name(ret));
			errors++;
		}
	} else if (ret != WOULD_HAVE_BLOCKED) {
		diag("Agent opening file gave %s",
		     agent_return_name(ret));
		errors++;
	}

	free(contents);
}

static void after_unlock(int fd)
{
	if (opened)
		check_file_intact(fd);
}

static ssize_t pwrite_check(int fd,
			    const void *buf, size_t count, off_t offset)
{
	if (opened)
		check_file_intact(fd);

	return pwrite(fd, buf, count, offset);
}

static ssize_t write_check(int fd, const void *buf, size_t count)
{
	if (opened)
		check_file_intact(fd);

	return write(fd, buf, count);
}

static int ftruncate_check(int fd, off_t length)
{
	if (opened)
		check_file_intact(fd);

	return ftruncate(fd, length);

}

int main(int argc, char *argv[])
{
	const int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	int i;
	struct ntdb_context *ntdb;
	NTDB_DATA key, data;

	plan_tests(sizeof(flags)/sizeof(flags[0]) * 5);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	unlock_callback = after_unlock;
	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		diag("Test with %s and %s\n",
		     (flags[i] & NTDB_CONVERT) ? "CONVERT" : "DEFAULT",
		     (flags[i] & NTDB_NOMMAP) ? "no mmap" : "mmap");
		unlink(TEST_DBNAME);
		ntdb = ntdb_open(TEST_DBNAME, flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(ntdb);

		opened = true;
		ok1(ntdb_transaction_start(ntdb) == 0);
		key = ntdb_mkdata("hi", strlen("hi"));
		data = ntdb_mkdata("world", strlen("world"));

		ok1(ntdb_store(ntdb, key, data, NTDB_INSERT) == 0);
		ok1(ntdb_transaction_commit(ntdb) == 0);
		ok(!errors, "We had %u open errors", errors);

		opened = false;
		ntdb_close(ntdb);
	}

	return exit_status();
}
