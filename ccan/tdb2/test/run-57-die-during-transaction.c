#include "config.h"
#include <unistd.h>
#include "lock-tracking.h"
#include <ccan/tap/tap.h>
#include <stdlib.h>
#include <assert.h>
static ssize_t pwrite_check(int fd, const void *buf, size_t count, off_t offset);
static ssize_t write_check(int fd, const void *buf, size_t count);
static int ftruncate_check(int fd, off_t length);

#define pwrite pwrite_check
#define write write_check
#define fcntl fcntl_with_lockcheck
#define ftruncate ftruncate_check

/* There's a malloc inside transaction_setup_recovery, and valgrind complains
 * when we longjmp and leak it. */
#define MAX_ALLOCATIONS 200
static void *allocated[MAX_ALLOCATIONS];

static void *malloc_noleak(size_t len)
{
	unsigned int i;

	for (i = 0; i < MAX_ALLOCATIONS; i++)
		if (!allocated[i]) {
			allocated[i] = malloc(len);
			return allocated[i];
		}
	diag("Too many allocations!");
	abort();
}

static void free_noleak(void *p)
{
	unsigned int i;

	/* We don't catch realloc, so don't care if we miss one. */
	for (i = 0; i < MAX_ALLOCATIONS; i++) {
		if (allocated[i] == p) {
			allocated[i] = NULL;
			break;
		}
	}
	free(p);
}

static void free_all(void)
{
	unsigned int i;

	for (i = 0; i < MAX_ALLOCATIONS; i++) {
		free(allocated[i]);
		allocated[i] = NULL;
	}
}

#define malloc malloc_noleak
#define free free_noleak

#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#undef malloc
#undef free
#undef write
#undef pwrite
#undef fcntl
#undef ftruncate

#include <stdbool.h>
#include <stdarg.h>
#include <err.h>
#include <setjmp.h>
#include "external-agent.h"
#include "logging.h"

static bool in_transaction;
static int target, current;
static jmp_buf jmpbuf;
#define TEST_DBNAME "run-57-die-during-transaction.tdb"
#define KEY_STRING "helloworld"

static void maybe_die(int fd)
{
	if (in_transaction && current++ == target) {
		longjmp(jmpbuf, 1);
	}
}

static ssize_t pwrite_check(int fd,
			    const void *buf, size_t count, off_t offset)
{
	ssize_t ret;

	maybe_die(fd);

	ret = pwrite(fd, buf, count, offset);
	if (ret != count)
		return ret;

	maybe_die(fd);
	return ret;
}

static ssize_t write_check(int fd, const void *buf, size_t count)
{
	ssize_t ret;

	maybe_die(fd);

	ret = write(fd, buf, count);
	if (ret != count)
		return ret;

	maybe_die(fd);
	return ret;
}

static int ftruncate_check(int fd, off_t length)
{
	int ret;

	maybe_die(fd);

	ret = ftruncate(fd, length);

	maybe_die(fd);
	return ret;
}

static bool test_death(enum operation op, struct agent *agent)
{
	struct tdb_context *tdb = NULL;
	TDB_DATA key;
	enum agent_return ret;
	int needed_recovery = 0;

	current = target = 0;
reset:
	unlink(TEST_DBNAME);
	tdb = tdb_open(TEST_DBNAME, TDB_NOMMAP,
		       O_CREAT|O_TRUNC|O_RDWR, 0600, &tap_log_attr);
	if (!tdb) {
		diag("Failed opening TDB: %s", strerror(errno));
		return false;
	}

	if (setjmp(jmpbuf) != 0) {
		/* We're partway through.  Simulate our death. */
		close(tdb->file->fd);
		forget_locking();
		in_transaction = false;

		ret = external_agent_operation(agent, NEEDS_RECOVERY, "");
		if (ret == SUCCESS)
			needed_recovery++;
		else if (ret != FAILED) {
			diag("Step %u agent NEEDS_RECOVERY = %s", current,
			     agent_return_name(ret));
			return false;
		}

		ret = external_agent_operation(agent, op, KEY_STRING);
		if (ret != SUCCESS) {
			diag("Step %u op %s failed = %s", current,
			     operation_name(op),
			     agent_return_name(ret));
			return false;
		}

		ret = external_agent_operation(agent, NEEDS_RECOVERY, "");
		if (ret != FAILED) {
			diag("Still needs recovery after step %u = %s",
			     current, agent_return_name(ret));
			return false;
		}

		ret = external_agent_operation(agent, CHECK, "");
		if (ret != SUCCESS) {
			diag("Step %u check failed = %s", current,
			     agent_return_name(ret));
			return false;
		}

		ret = external_agent_operation(agent, CLOSE, "");
		if (ret != SUCCESS) {
			diag("Step %u close failed = %s", current,
			     agent_return_name(ret));
			return false;
		}

		/* Suppress logging as this tries to use closed fd. */
		suppress_logging = true;
		suppress_lockcheck = true;
		tdb_close(tdb);
		suppress_logging = false;
		suppress_lockcheck = false;
		target++;
		current = 0;
		free_all();
		goto reset;
	}

	/* Put key for agent to fetch. */
	key = tdb_mkdata(KEY_STRING, strlen(KEY_STRING));
	if (tdb_store(tdb, key, key, TDB_INSERT) != 0)
		return false;

	/* This is the key we insert in transaction. */
	key.dsize--;

	ret = external_agent_operation(agent, OPEN, TEST_DBNAME);
	if (ret != SUCCESS)
		errx(1, "Agent failed to open: %s", agent_return_name(ret));

	ret = external_agent_operation(agent, FETCH, KEY_STRING);
	if (ret != SUCCESS)
		errx(1, "Agent failed find key: %s", agent_return_name(ret));

	in_transaction = true;
	if (tdb_transaction_start(tdb) != 0)
		return false;

	if (tdb_store(tdb, key, key, TDB_INSERT) != 0)
		return false;

	if (tdb_transaction_commit(tdb) != 0)
		return false;

	in_transaction = false;

	/* We made it! */
	diag("Completed %u runs", current);
	tdb_close(tdb);
	ret = external_agent_operation(agent, CLOSE, "");
	if (ret != SUCCESS) {
		diag("Step %u close failed = %s", current,
		     agent_return_name(ret));
		return false;
	}

	ok1(needed_recovery);
	ok1(locking_errors == 0);
	ok1(forget_locking() == 0);
	locking_errors = 0;
	return true;
}

int main(int argc, char *argv[])
{
	enum operation ops[] = { FETCH, STORE, TRANSACTION_START };
	struct agent *agent;
	int i;

	plan_tests(12);
	unlock_callback = maybe_die;

	agent = prepare_external_agent();
	if (!agent)
		err(1, "preparing agent");

	for (i = 0; i < sizeof(ops)/sizeof(ops[0]); i++) {
		diag("Testing %s after death", operation_name(ops[i]));
		ok1(test_death(ops[i], agent));
	}

	free_external_agent(agent);
	return exit_status();
}
