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
#include <ccan/tdb/check.c>
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include <setjmp.h>
#include "external-transaction.h"

#undef write
#undef pwrite
#undef fcntl
#undef ftruncate

static bool in_transaction;
static bool suppress_logging;
static int target, current;
static jmp_buf jmpbuf;
#define TEST_DBNAME "/tmp/test7.tdb"

/* We save the locks so we can reaquire them. */
struct lock {
	struct lock *next;
	off_t off;
	unsigned int len;
	int type;
};
static struct lock *locks;

static void taplog(struct tdb_context *tdb,
		   enum tdb_debug_level level,
		   const char *fmt, ...)
{
	va_list ap;
	char line[200];

	if (suppress_logging)
		return;

	va_start(ap, fmt);
	vsprintf(line, fmt, ap);
	va_end(ap);

	diag("%s", line);
}

static void check_file_contents(int fd)
{
	if (current++ == target) {
		longjmp(jmpbuf, 1);
	}
}

static ssize_t pwrite_check(int fd,
			    const void *buf, size_t count, off_t offset)
{
	ssize_t ret;

	if (in_transaction)
		check_file_contents(fd);

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

	ret = write(fd, buf, count);
	if (ret != count)
		return ret;

	if (in_transaction)
		check_file_contents(fd);
	return ret;
}

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
	if (ret == 0) {
		if (fl->l_type == F_UNLCK) {
			struct lock **l;
			struct lock *old = NULL;
			
			for (l = &locks; *l; l = &(*l)->next) {
				if ((*l)->off == fl->l_start
				    && (*l)->len == fl->l_len) {
					old = *l;
					*l = (*l)->next;
					free(old);
					break;
				}
			}
			if (!old)
				errx(1, "Unknown lock");
		} else {
			struct lock *new = malloc(sizeof *new);
			new->off = fl->l_start;
			new->len = fl->l_len;
			new->type = fl->l_type;
			new->next = locks;
			locks = new;
		}
	}

	if (in_transaction && fl->l_type == F_UNLCK)
		check_file_contents(fd);
	return ret;
}

static int ftruncate_check(int fd, off_t length)
{
	int ret;

	if (in_transaction)
		check_file_contents(fd);

	ret = ftruncate(fd, length);

	if (in_transaction)
		check_file_contents(fd);
	return ret;
}

static bool test_death(enum operation op, struct agent *agent)
{
	struct tdb_context *tdb;
	TDB_DATA key, data;
	struct tdb_logging_context logctx = { taplog, NULL };
	int needed_recovery = 0;

	current = target = 0;
reset:
	if (setjmp(jmpbuf) != 0) {
		/* We're partway through.  Simulate our death. */
		close(tdb->fd);
		in_transaction = false;

		if (external_agent_operation(agent, NEEDS_RECOVERY_KEEP_OPENED,
					     ""))
			needed_recovery++;

		if (external_agent_operation(agent, op, "") != 1) {
			diag("Step %u op failed", current);
			return false;
		}

		if (external_agent_operation(agent, NEEDS_RECOVERY_KEEP_OPENED,
					     "")) {
			diag("Still needs recovery after step %u", current);
			return false;
		}

		if (external_agent_operation(agent, CHECK_KEEP_OPENED, "")
		    != 1) {
			diag("Step %u check failed", current);
			return false;
		}

		external_agent_operation(agent, CLOSE, "");
		/* Suppress logging as this tries to use closed fd. */
		suppress_logging = true;
		tdb_close(tdb);
		suppress_logging = false;
		target++;
		current = 0;
		goto reset;
	}

	unlink(TEST_DBNAME);
	tdb = tdb_open_ex(TEST_DBNAME, 1024, TDB_NOMMAP,
			  O_CREAT|O_TRUNC|O_RDWR, 0600, &logctx, NULL);

	if (external_agent_operation(agent, KEEP_OPENED, TEST_DBNAME) != 0)
		errx(1, "Agent failed to open?");

	if (tdb_transaction_start(tdb) != 0)
		return false;

	in_transaction = true;
	key.dsize = strlen("hi");
	key.dptr = (void *)"hi";
	data.dptr = (void *)"world";
	data.dsize = strlen("world");

	if (tdb_store(tdb, key, data, TDB_INSERT) != 0)
		return false;
	if (tdb_transaction_commit(tdb) != 0)
		return false;

	in_transaction = false;

	/* We made it! */
	diag("Completed %u runs", current);
	tdb_close(tdb);
	external_agent_operation(agent, CLOSE, "");

	ok1(needed_recovery);
	return true;
}

int main(int argc, char *argv[])
{
	enum operation ops[] = { FETCH_KEEP_OPENED,
				 STORE_KEEP_OPENED,
				 TRANSACTION_KEEP_OPENED };
	struct agent *agent;
	int i;

	plan_tests(6);
	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	/* Nice ourselves down: we can't tell the difference between agent
	 * blocking on lock, and agent not scheduled. */
	nice(15);

	for (i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
		diag("Testing %s after death",
		     ops[i] == TRANSACTION_KEEP_OPENED ? "transaction"
		     : ops[i] == FETCH_KEEP_OPENED ? "fetch"
		     : ops[i] == STORE_KEEP_OPENED ? "store"
		     : NULL);

		ok1(test_death(ops[i], agent));
	}

	return exit_status();
}
