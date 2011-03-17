#include "failtest_helper.h"
#include "logging.h"
#include <string.h>
#include <ccan/tap/tap.h>

/* FIXME: From ccan/str */
static inline bool strends(const char *str, const char *postfix)
{
	if (strlen(str) < strlen(postfix))
		return false;

	return !strcmp(str + strlen(str) - strlen(postfix), postfix);
}

bool failmatch(const struct failtest_call *call,
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

static bool is_unlock(const struct failtest_call *call)
{
	return call->type == FAILTEST_FCNTL
		&& call->u.fcntl.arg.fl.l_type == F_UNLCK;
}

bool exit_check_log(struct failtest_call *history, unsigned num)
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

/* Some places we soldier on despite errors: only fail them once. */
enum failtest_result
block_repeat_failures(struct failtest_call *history, unsigned num)
{
	const struct failtest_call *i, *last = &history[num-1];

	if (failmatch(last, INITIAL_TDB_MALLOC)
	    || failmatch(last, URANDOM_OPEN)
	    || failmatch(last, URANDOM_READ)) {
		if (find_repeat(history, last, last))
			return FAIL_DONT_FAIL;
		return FAIL_PROBE;
	}

	/* Unlock or non-blocking lock is fail-once. */
	if (is_unlock(last)) {
		/* Find a previous unlock at this point? */
		for (i = find_repeat(history, last, last);
		     i;
		     i = find_repeat(history, i, last)) {
			if (is_unlock(i))
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
