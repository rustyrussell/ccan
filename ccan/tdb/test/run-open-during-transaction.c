#define _XOPEN_SOURCE 500
#include <unistd.h>
#include "lock-tracking.h"

static ssize_t pwrite_check(int fd, const void *buf, size_t count, off_t offset);
static ssize_t write_check(int fd, const void *buf, size_t count);
static int ftruncate_check(int fd, off_t length);

#define pwrite pwrite_check
#define write write_check
#define fcntl fcntl_with_lockcheck
#define ftruncate ftruncate_check

#include <ccan/tdb/tdb.h>
#include <ccan/tdb/io.c>
#include <ccan/tdb/tdb.c>
#include <ccan/tdb/lock.c>
#include <ccan/tdb/freelist.c>
#include <ccan/tdb/traverse.c>
#include <ccan/tdb/transaction.c>
#include <ccan/tdb/error.c>
#include <ccan/tdb/open.c>
#include <ccan/tdb/check.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include "external-transaction.h"

static struct agent *agent;
static bool agent_pending;
static bool in_transaction;
static int errors = 0;
static bool snapshot_uptodate;
static char *snapshot;
static off_t snapshot_len;
static bool clear_if_first;
#define TEST_DBNAME "run-open-during-transaction.tdb"

#undef write
#undef pwrite
#undef fcntl
#undef ftruncate

static void taplog(struct tdb_context *tdb,
		   enum tdb_debug_level level,
		   const char *fmt, ...)
{
	va_list ap;
	char line[200];

	va_start(ap, fmt);
	vsprintf(line, fmt, ap);
	va_end(ap);

	diag("%s", line);
}

static void save_file_contents(int fd)
{
	struct stat st;
	int res;

	/* Save copy of file. */
	stat(TEST_DBNAME, &st);
	if (snapshot_len != st.st_size) {
		snapshot = realloc(snapshot, st.st_size * 2);
		snapshot_len = st.st_size;
	}
	res = pread(fd, snapshot, snapshot_len, 0);
	if (res != snapshot_len)
		err(1, "Reading %zu bytes = %u", (size_t)snapshot_len, res);
	snapshot_uptodate = true;
}

static void check_for_agent(int fd, bool block)
{
	struct stat st;
	int res;

	if (!external_agent_operation_check(agent, block, &res))
		return;

	if (res != 1)
		err(1, "Agent failed open");
	agent_pending = false;

	if (!snapshot_uptodate)
		return;

	stat(TEST_DBNAME, &st);
	if (st.st_size != snapshot_len) {
		diag("Other open changed size from %zu -> %zu",
		     (size_t)snapshot_len, (size_t)st.st_size);
		errors++;
		return;
	}

	if (pread(fd, snapshot+snapshot_len, snapshot_len, 0) != snapshot_len)
		err(1, "Reading %zu bytes", (size_t)snapshot_len);
	if (memcmp(snapshot, snapshot+snapshot_len, snapshot_len) != 0) {
		diag("File changed");
		errors++;
		return;
	}
}

static void check_file_contents(int fd)
{
	if (!in_transaction)
		return;

	if (agent_pending)
		check_for_agent(fd, false);

	if (!agent_pending) {
		save_file_contents(fd);

		/* Ask agent to open file. */
		external_agent_operation_start(agent,
					       clear_if_first ?
					       OPEN_WITH_CLEAR_IF_FIRST :
					       OPEN,
					       TEST_DBNAME);
		agent_pending = true;
		/* Hack: give it a chance to run. */
		sleep(0);
	}

	check_for_agent(fd, false);
}

static ssize_t pwrite_check(int fd,
			    const void *buf, size_t count, off_t offset)
{
	ssize_t ret;

	check_file_contents(fd);

	snapshot_uptodate = false;
	ret = pwrite(fd, buf, count, offset);
	if (ret != count)
		return ret;

	check_file_contents(fd);
	return ret;
}

static ssize_t write_check(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	check_file_contents(fd);

	snapshot_uptodate = false;

	ret = write(fd, buf, count);
	if (ret != count)
		return ret;

	check_file_contents(fd);
	return ret;
}

static int ftruncate_check(int fd, off_t length)
{
	int ret;

	check_file_contents(fd);

	snapshot_uptodate = false;

	ret = ftruncate(fd, length);

	check_file_contents(fd);
	return ret;
}

int main(int argc, char *argv[])
{
	struct tdb_logging_context logctx = { taplog, NULL };
	const int flags[] = { TDB_DEFAULT,
			      TDB_CLEAR_IF_FIRST,
			      TDB_NOMMAP, 
			      TDB_CLEAR_IF_FIRST | TDB_NOMMAP };
	int i;
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(20);
	unlock_callback = check_file_contents;
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	/* Nice ourselves down: we can't tell the difference between agent
	 * blocking on lock, and agent not scheduled. */
	nice(15);

	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		clear_if_first = (flags[i] & TDB_CLEAR_IF_FIRST);
		diag("Test with %s and %s\n",
		     clear_if_first ? "CLEAR" : "DEFAULT",
		     (flags[i] & TDB_NOMMAP) ? "no mmap" : "mmap");
		unlink(TEST_DBNAME);
		tdb = tdb_open_ex(TEST_DBNAME, 1024, flags[i],
				  O_CREAT|O_TRUNC|O_RDWR, 0600,
				  &logctx, NULL);
		ok1(tdb);

		ok1(tdb_transaction_start(tdb) == 0);
		in_transaction = true;
		key.dsize = strlen("hi");
		key.dptr = (void *)"hi";
		data.dptr = (void *)"world";
		data.dsize = strlen("world");

		ok1(tdb_store(tdb, key, data, TDB_INSERT) == 0);
		ok1(tdb_transaction_commit(tdb) == 0);
		if (agent_pending)
			check_for_agent(tdb->fd, true);
		ok(errors == 0, "We had %u unexpected changes", errors);

		tdb_close(tdb);
	}

	return exit_status();
}
