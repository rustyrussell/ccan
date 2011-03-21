#include "config.h"
#include <unistd.h>
#include "lock-tracking.h"

static ssize_t pwrite_check(int fd, const void *buf, size_t count, off_t offset);
static ssize_t write_check(int fd, const void *buf, size_t count);
static int ftruncate_check(int fd, off_t length);

#define pwrite pwrite_check
#define write write_check
#define fcntl fcntl_with_lockcheck
#define ftruncate ftruncate_check

#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include "external-agent.h"
#include "logging.h"

static struct agent *agent;
static bool opened;
static int errors = 0;
#define TEST_DBNAME "run-56-open-during-transaction.tdb"

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
	bool same;

	/* over-length read serves as length check. */
	contents = malloc(snapshot_len+1);
	same = pread(fd, contents, snapshot_len+1, 0) == snapshot_len
		&& is_same(snapshot, contents, snapshot_len);
	free(contents);
	return same;
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
			diag("Agent failed to close tdb: %s",
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
	const int flags[] = { TDB_DEFAULT,
			      TDB_NOMMAP,
			      TDB_CONVERT,
			      TDB_CONVERT | TDB_NOMMAP };
	int i;
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(20);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	unlock_callback = after_unlock;
	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		diag("Test with %s and %s\n",
		     (flags[i] & TDB_CONVERT) ? "CONVERT" : "DEFAULT",
		     (flags[i] & TDB_NOMMAP) ? "no mmap" : "mmap");
		unlink(TEST_DBNAME);
		tdb = tdb_open(TEST_DBNAME, flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		ok1(tdb);

		opened = true;
		ok1(tdb_transaction_start(tdb) == 0);
		key = tdb_mkdata("hi", strlen("hi"));
		data = tdb_mkdata("world", strlen("world"));

		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_transaction_commit(tdb) == 0);
		ok(!errors, "We had %u open errors", errors);

		opened = false;
		tdb_close(tdb);
	}

	return exit_status();
}
