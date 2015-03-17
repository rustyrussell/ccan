#include "external-agent.h"
#include "logging.h"
#include "lock-tracking.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ccan/err/err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "tap-interface.h"
#include <stdio.h>
#include <stdarg.h>

static struct ntdb_context *ntdb;

void (*external_agent_free)(void *) = free;

static enum NTDB_ERROR clear_if_first(int fd, void *arg)
{
/* We hold a lock offset 4 always, so we can tell if anyone is holding it.
 * (This is compatible with tdb's TDB_CLEAR_IF_FIRST flag).  */
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 4;
	fl.l_len = 1;

	if (fcntl(fd, F_SETLK, &fl) == 0) {
		/* We must be first ones to open it! */
		diag("agent truncating file!");
		if (ftruncate(fd, 0) != 0) {
			return NTDB_ERR_IO;
		}
	}
	fl.l_type = F_RDLCK;
	if (fcntl(fd, F_SETLKW, &fl) != 0) {
		return NTDB_ERR_IO;
	}
	return NTDB_SUCCESS;
}

static enum agent_return do_operation(enum operation op, const char *name)
{
	NTDB_DATA k, d;
	enum agent_return ret;
	NTDB_DATA data;
	enum NTDB_ERROR ecode;
	union ntdb_attribute cif;
	const char *eq;

	if (op != OPEN && op != OPEN_WITH_HOOK && !ntdb) {
		diag("external: No ntdb open!");
		return OTHER_FAILURE;
	}

	diag("external: %s", operation_name(op));

	eq = strchr(name, '=');
	if (eq) {
		k = ntdb_mkdata(name, eq - name);
		d = ntdb_mkdata(eq + 1, strlen(eq+1));
	} else {
		k = ntdb_mkdata(name, strlen(name));
		d.dsize = 0;
		d.dptr = NULL;
	}

	locking_would_block = 0;
	switch (op) {
	case OPEN:
		if (ntdb) {
			diag("Already have ntdb %s open", ntdb_name(ntdb));
			return OTHER_FAILURE;
		}
		ntdb = ntdb_open(name, MAYBE_NOSYNC, O_RDWR, 0, &tap_log_attr);
		if (!ntdb) {
			if (!locking_would_block)
				diag("Opening ntdb gave %s", strerror(errno));
			forget_locking();
			ret = OTHER_FAILURE;
		} else
			ret = SUCCESS;
		break;
	case OPEN_WITH_HOOK:
		if (ntdb) {
			diag("Already have ntdb %s open", ntdb_name(ntdb));
			return OTHER_FAILURE;
		}
		cif.openhook.base.attr = NTDB_ATTRIBUTE_OPENHOOK;
		cif.openhook.base.next = &tap_log_attr;
		cif.openhook.fn = clear_if_first;
		ntdb = ntdb_open(name, MAYBE_NOSYNC, O_RDWR, 0, &cif);
		if (!ntdb) {
			if (!locking_would_block)
				diag("Opening ntdb gave %s", strerror(errno));
			forget_locking();
			ret = OTHER_FAILURE;
		} else
			ret = SUCCESS;
		break;
	case FETCH:
		ecode = ntdb_fetch(ntdb, k, &data);
		if (ecode == NTDB_ERR_NOEXIST) {
			ret = FAILED;
		} else if (ecode < 0) {
			ret = OTHER_FAILURE;
		} else if (!ntdb_deq(data, d)) {
			ret = OTHER_FAILURE;
			external_agent_free(data.dptr);
		} else {
			ret = SUCCESS;
			external_agent_free(data.dptr);
		}
		break;
	case STORE:
		ret = ntdb_store(ntdb, k, d, 0) == 0 ? SUCCESS : OTHER_FAILURE;
		break;
	case TRANSACTION_START:
		ret = ntdb_transaction_start(ntdb) == 0 ? SUCCESS : OTHER_FAILURE;
		break;
	case TRANSACTION_COMMIT:
		ret = ntdb_transaction_commit(ntdb)==0 ? SUCCESS : OTHER_FAILURE;
		break;
	case NEEDS_RECOVERY:
		ret = external_agent_needs_rec(ntdb);
		break;
	case CHECK:
		ret = ntdb_check(ntdb, NULL, NULL) == 0 ? SUCCESS : OTHER_FAILURE;
		break;
	case CLOSE:
		ret = ntdb_close(ntdb) == 0 ? SUCCESS : OTHER_FAILURE;
		ntdb = NULL;
		break;
	case SEND_SIGNAL:
		/* We do this async */
		ret = SUCCESS;
		break;
	default:
		ret = OTHER_FAILURE;
	}

	if (locking_would_block)
		ret = WOULD_HAVE_BLOCKED;

	return ret;
}

struct agent {
	int cmdfd, responsefd;
};

/* Do this before doing any ntdb stuff.  Return handle, or NULL. */
struct agent *prepare_external_agent(void)
{
	int pid, ret;
	int command[2], response[2];
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

	/* We want to fail, not block. */
	nonblocking_locks = true;
	log_prefix = "external: ";
	while ((ret = read(command[0], name, sizeof(name))) > 0) {
		enum agent_return result;

		result = do_operation(name[0], name+1);
		if (write(response[1], &result, sizeof(result))
		    != sizeof(result))
			err(1, "Writing response");
		if (name[0] == SEND_SIGNAL) {
			struct timeval ten_ms;
			ten_ms.tv_sec = 0;
			ten_ms.tv_usec = 10000;
			select(0, NULL, NULL, NULL, &ten_ms);
			kill(getppid(), SIGUSR1);
		}
	}
	exit(0);
}

/* Ask the external agent to try to do an operation. */
enum agent_return external_agent_operation(struct agent *agent,
					   enum operation op,
					   const char *name)
{
	enum agent_return res;
	unsigned int len;
	char *string;

	if (!name)
		name = "";
	len = 1 + strlen(name) + 1;
	string = malloc(len);

	string[0] = op;
	strcpy(string+1, name);

	if (write(agent->cmdfd, string, len) != len
	    || read(agent->responsefd, &res, sizeof(res)) != sizeof(res))
		res = AGENT_DIED;

	free(string);
	return res;
}

const char *agent_return_name(enum agent_return ret)
{
	return ret == SUCCESS ? "SUCCESS"
		: ret == WOULD_HAVE_BLOCKED ? "WOULD_HAVE_BLOCKED"
		: ret == AGENT_DIED ? "AGENT_DIED"
		: ret == FAILED ? "FAILED"
		: ret == OTHER_FAILURE ? "OTHER_FAILURE"
		: "**INVALID**";
}

const char *operation_name(enum operation op)
{
	switch (op) {
	case OPEN: return "OPEN";
	case OPEN_WITH_HOOK: return "OPEN_WITH_HOOK";
	case FETCH: return "FETCH";
	case STORE: return "STORE";
	case CHECK: return "CHECK";
	case TRANSACTION_START: return "TRANSACTION_START";
	case TRANSACTION_COMMIT: return "TRANSACTION_COMMIT";
	case NEEDS_RECOVERY: return "NEEDS_RECOVERY";
	case SEND_SIGNAL: return "SEND_SIGNAL";
	case CLOSE: return "CLOSE";
	}
	return "**INVALID**";
}

void free_external_agent(struct agent *agent)
{
	close(agent->cmdfd);
	close(agent->responsefd);
	free(agent);
}
