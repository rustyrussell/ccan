#include <ccan/failtest/failtest_override.h>
#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/check.c>
#include <ccan/tap/tap.h>
#include <ccan/failtest/failtest.h>
#include "logging.h"

/* FIXME: Check these! */
#define INITIAL_TDB_MALLOC	"tdb.c", 177, FAILTEST_MALLOC
#define LOGGING_MALLOC		"tdb.c", 734, FAILTEST_MALLOC
#define URANDOM_OPEN		"tdb.c", 49, FAILTEST_OPEN
#define URANDOM_READ		"tdb.c", 29, FAILTEST_READ

static bool failmatch(const struct failtest_call *call,
		      const char *file, int line, enum failtest_call_type type)
{
	return call->type == type
		&& call->line == line
		&& ((strcmp(call->file, file) == 0)
		    || (strends(call->file, file)
			&& (call->file[strlen(call->file) - strlen(file) - 1]
			    == '/')));
}

static const struct failtest_call *
find_repeat(const struct failtest_call *start, const struct failtest_call *end,
	    const struct failtest_call *call)
{
	const struct failtest_call *i;

	for (i = start; i < end; i++) {
		if (failmatch(i, call->file, call->line, call->type))
			return i;
	}
	return NULL;
}

static bool is_nonblocking_lock(const struct failtest_call *call)
{
	return call->type == FAILTEST_FCNTL && call->u.fcntl.cmd == F_SETLK;
}

/* Some places we soldier on despite errors: only fail them once. */
static enum failtest_result
block_repeat_failures(struct failtest_call *history, unsigned num)
{
	const struct failtest_call *i, *last = &history[num-1];

	if (failmatch(last, INITIAL_TDB_MALLOC)
	    || failmatch(last, LOGGING_MALLOC)
	    || failmatch(last, URANDOM_OPEN)
	    || failmatch(last, URANDOM_READ)) {
		if (find_repeat(history, last, last))
			return FAIL_DONT_FAIL;
		return FAIL_PROBE;
	}

	/* Unlock or non-blocking lock is fail-once. */
	if (last->type == FAILTEST_FCNTL
	    && last->u.fcntl.arg.fl.l_type == F_UNLCK) {
		/* Find a previous unlock at this point? */
		for (i = find_repeat(history, last, last);
		     i;
		     i = find_repeat(history, i, last)) {
			if (i->u.fcntl.arg.fl.l_type == F_UNLCK)
				return FAIL_DONT_FAIL;
		}
		return FAIL_PROBE;
	} else if (is_nonblocking_lock(last)) {
		/* Find a previous non-blocking lock at this point? */
		for (i = find_repeat(history, last, last);
		     i;
		     i = find_repeat(history, i, last)) {
			if (is_nonblocking_lock(i))
				return FAIL_DONT_FAIL;
		}
		return FAIL_PROBE;
	}

	return FAIL_OK;
}

static bool exit_check(struct failtest_call *history, unsigned num)
{
	unsigned int i;

	for (i = 0; i < num; i++) {
		if (!history[i].fail)
			continue;
		/* Failing the /dev/urandom open doesn't count: we fall back. */
		if (failmatch(&history[i], URANDOM_OPEN))
			continue;

		/* Similarly with read fail. */
		if (failmatch(&history[i], URANDOM_READ))
			continue;

		/* Initial allocation of tdb doesn't log. */
		if (failmatch(&history[i], INITIAL_TDB_MALLOC))
			continue;

		/* We don't block "failures" on non-blocking locks. */
		if (is_nonblocking_lock(&history[i]))
			continue;

		if (!tap_log_messages)
			diag("We didn't log for %u (%s:%u)",
			     i, history[i].file, history[i].line);
		return tap_log_messages != 0;
	}
	return true;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_INTERNAL, TDB_DEFAULT, TDB_NOMMAP,
			TDB_INTERNAL|TDB_CONVERT, TDB_CONVERT, 
			TDB_NOMMAP|TDB_CONVERT };

	failtest_init(argc, argv);
	failtest_hook = block_repeat_failures;
	failtest_exit_check = exit_check;
	plan_tests(sizeof(flags) / sizeof(flags[0]) * 3);
	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		tdb = tdb_open("run-new_database.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			failtest_exit(exit_status());
		if (tdb) {
			bool ok = ok1(tdb_check(tdb, NULL, NULL) == 0);
			tdb_close(tdb);
			if (!ok)
				failtest_exit(exit_status());
		}
		if (!ok1(tap_log_messages == 0))
			break;
	}
	failtest_exit(exit_status());
}
