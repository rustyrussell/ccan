#include "external-transaction.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <ccan/tdb/tdb.h>
#include <ccan/tdb/tdb_private.h>
#include <ccan/tap/tap.h>
#include <stdio.h>
#include <stdarg.h>

static struct tdb_context *tdb;

static volatile sig_atomic_t alarmed;
static void do_alarm(int signum)
{
	alarmed++;
}

static void taplog(struct tdb_context *tdb,
		   enum tdb_debug_level level,
		   const char *fmt, ...)
{
	va_list ap;
	char line[200];

	va_start(ap, fmt);
	vsprintf(line, fmt, ap);
	va_end(ap);

	diag("external: %s", line);
}

static int do_operation(enum operation op, const char *name)
{
	struct tdb_logging_context logctx = { taplog, NULL };

	TDB_DATA k = { .dptr = (void *)"a", .dsize = 1 };
	TDB_DATA d = { .dptr = (void *)"b", .dsize = 1 };

	if (op <= KEEP_OPENED) {
		tdb = tdb_open_ex(name, 0, op == OPEN_WITH_CLEAR_IF_FIRST ?
				  TDB_CLEAR_IF_FIRST : TDB_DEFAULT, O_RDWR, 0,
				  &logctx, NULL);
		if (!tdb)
			return -1;
	}

	if (op == KEEP_OPENED) {
		return 0;
	} else if (op == OPEN || op == OPEN_WITH_CLEAR_IF_FIRST || op == CLOSE) {
		tdb_close(tdb);
		tdb = NULL;
		return 1;
	} else if (op == STORE_KEEP_OPENED) {
		if (tdb_store(tdb, k, d, 0) != 0)
			return -2;
		return 1;
	} else if (op == FETCH_KEEP_OPENED) {
		TDB_DATA ret;
		ret = tdb_fetch(tdb, k);
		if (ret.dptr == NULL) {
			if (tdb_error(tdb) == TDB_ERR_NOEXIST)
				return 1;
			return -3;
		}
		if (ret.dsize != 1 || *(char *)ret.dptr != 'b')
			return -4;
		free(ret.dptr);
		return 1;
	} else if (op == CHECK_KEEP_OPENED) {
		return tdb_check(tdb, NULL, 0) == 0;
	} else if (op == NEEDS_RECOVERY_KEEP_OPENED) {
#if 0
		return tdb_maybe_needs_recovery(tdb);
#else
		return 0;
#endif
	}

	alarmed = 0;
	tdb_setalarm_sigptr(tdb, &alarmed);

	alarm(1);
	if (tdb_transaction_start(tdb) != 0)
		goto maybe_alarmed;

	alarm(0);
	if (tdb_store(tdb, k, d, 0) != 0) {
		tdb_transaction_cancel(tdb);
		tdb_close(tdb);
		tdb = NULL;
		return -2;
	}

	if (tdb_transaction_commit(tdb) == 0) {
		tdb_delete(tdb, k);
		if (op != TRANSACTION_KEEP_OPENED) {
			tdb_close(tdb);
			tdb = NULL;
		}
		return 1;
	}

	tdb_delete(tdb, k);
maybe_alarmed:
	if (op != TRANSACTION_KEEP_OPENED) {
		tdb_close(tdb);
		tdb = NULL;
	}
	if (alarmed)
		return 0;
	return -3;
}

struct agent {
	int cmdfd, responsefd;
};

/* Do this before doing any tdb stuff.  Return handle, or NULL. */
struct agent *prepare_external_agent(void)
{
	int pid, ret;
	int command[2], response[2];
	struct sigaction act = { .sa_handler = do_alarm };
	char name[1+PATH_MAX];

	if (pipe(command) != 0 || pipe(response) != 0)
		return NULL;

	pid = fork();
	if (pid < 0)
		return NULL;

	if (pid != 0) {
		struct agent *agent = malloc(sizeof(*agent));

		close(command[0]);
		close(response[1]);
		agent->cmdfd = command[1];
		agent->responsefd = response[0];
		return agent;
	}

	close(command[1]);
	close(response[0]);
	sigaction(SIGALRM, &act, NULL);

	while ((ret = read(command[0], name, sizeof(name))) > 0) {
		int result;

		result = do_operation(name[0], name+1);
		if (write(response[1], &result, sizeof(result))
		    != sizeof(result))
			err(1, "Writing response");
	}
	diag("external: read %i: %s", ret, strerror(errno));
	exit(0);
}

/* Ask the external agent to try to do an operation. */
int external_agent_operation(struct agent *agent,
			      enum operation op, const char *tdbname)
{
	int res;
	char string[1 + strlen(tdbname) + 1];

	string[0] = op;
	strcpy(string+1, tdbname);

	if (write(agent->cmdfd, string, sizeof(string)) != sizeof(string))
		err(1, "Writing to agent");

	if (read(agent->responsefd, &res, sizeof(res)) != sizeof(res))
		err(1, "Reading from agent");

	if (res > 1)
		errx(1, "Agent returned %u\n", res);

	return res;
}

void external_agent_operation_start(struct agent *agent,
				    enum operation op, const char *tdbname)
{
	char string[1 + strlen(tdbname) + 1];

	string[0] = op;
	strcpy(string+1, tdbname);

	if (write(agent->cmdfd, string, sizeof(string)) != sizeof(string))
		err(1, "Writing to agent");
}

bool external_agent_operation_check(struct agent *agent, bool block, int *res)
{
	int flags = fcntl(agent->responsefd, F_GETFL);

	if (block)
		fcntl(agent->responsefd, F_SETFL, flags & ~O_NONBLOCK);
	else
		fcntl(agent->responsefd, F_SETFL, flags | O_NONBLOCK);

	switch (read(agent->responsefd, res, sizeof(*res))) {
	case sizeof(*res):
		break;
	case 0:
		errx(1, "Agent died?");
	default:
		if (!block && (errno == EAGAIN || errno == EWOULDBLOCK))
			return false;
		err(1, "%slocking reading from agent", block ? "B" : "Non-b");
	}

	if (*res > 1)
		errx(1, "Agent returned %u\n", *res);

	return true;
}
