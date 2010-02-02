#define _XOPEN_SOURCE 500
#include <unistd.h>
static ssize_t pwrite_check(int fd, const void *buf, size_t count, off_t offset);
static ssize_t write_check(int fd, const void *buf, size_t count);
static int fcntl_check(int fd, int cmd, ... /* arg */ );
static int ftruncate_check(int fd, off_t length);

#define pwrite pwrite_check
#define write write_check
#define fcntl fcntl_check
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
#define TEST_DBNAME "/tmp/test7.tdb"

#undef write
#undef pwrite
#undef fcntl
#undef ftruncate

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

	if (res != 0)
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

	if (in_transaction)
		check_file_contents(fd);

	snapshot_uptodate = false;
	ret = pwrite(fd, buf, count, offset);
	if (ret != count)
		return ret;

	if (in_transaction)
		check_file_contents(fd);
	return ret;
}

static ssize_t write_check(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	if (in_transaction)
		check_file_contents(fd);

	snapshot_uptodate = false;

	ret = write(fd, buf, count);
	if (ret != count)
		return ret;

	if (in_transaction)
		check_file_contents(fd);
	return ret;
}

/* This seems to be a macro for glibc... */
extern int fcntl(int fd, int cmd, ... /* arg */ );

static int fcntl_check(int fd, int cmd, ... /* arg */ )
{
	va_list ap;
	int ret, arg3;
	struct flock *fl;

	if (cmd != F_SETLK && cmd != F_SETLKW) {
		/* This may be totally bogus, but we don't know in general. */
		va_start(ap, cmd);
		arg3 = va_arg(ap, int);
		va_end(ap);

		return fcntl(fd, cmd, arg3);
	}

	va_start(ap, cmd);
	fl = va_arg(ap, struct flock *);
	va_end(ap);

	ret = fcntl(fd, cmd, fl);

	if (in_transaction && fl->l_type == F_UNLCK)
		check_file_contents(fd);
	return ret;
}

static int ftruncate_check(int fd, off_t length)
{
	int ret;

	if (in_transaction)
		check_file_contents(fd);

	snapshot_uptodate = false;

	ret = ftruncate(fd, length);

	if (in_transaction)
		check_file_contents(fd);
	return ret;
}

int main(int argc, char *argv[])
{
	const int flags[] = { TDB_DEFAULT,
			      TDB_CLEAR_IF_FIRST,
			      TDB_NOMMAP, 
			      TDB_CLEAR_IF_FIRST | TDB_NOMMAP };
	int i;
	struct tdb_context *tdb;
	TDB_DATA key, data;

	plan_tests(20);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	/* Nice ourselves down: we can't tell the difference between agent
	 * blocking on lock, and agent not scheduled. */
	nice(15);

	for (i = 0; i < sizeof(flags)/sizeof(flags[0]); i++) {
		unlink(TEST_DBNAME);
		tdb = tdb_open(TEST_DBNAME, 1024, flags[i],
			       O_CREAT|O_TRUNC|O_RDWR, 0600);
		ok1(tdb);
		clear_if_first = (flags[i] & TDB_CLEAR_IF_FIRST);

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
		ok(errors == 0, "We had %u errors", errors);

		tdb_close(tdb);
	}

	return exit_status();
}
