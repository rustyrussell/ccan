#include <ccan/tdb2/tdb2.h>
#include <ccan/tap/tap.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include "logging.h"
#include "external-agent.h"

#undef alarm
#define alarm fast_alarm

/* Speed things up by doing things in milliseconds. */
static unsigned int fast_alarm(unsigned int milli_seconds)
{
	struct itimerval it;

	it.it_interval.tv_sec = it.it_interval.tv_usec = 0;
	it.it_value.tv_sec = milli_seconds / 1000;
	it.it_value.tv_usec = milli_seconds * 1000;
	setitimer(ITIMER_REAL, &it, NULL);
	return 0;
}

#define CatchSignal(sig, handler) signal((sig), (handler))

static void do_nothing(int signum)
{
}

/* This example code is taken from SAMBA, so try not to change it. */
static struct flock flock_struct;

/* Return a value which is none of v1, v2 or v3. */
static inline short int invalid_value(short int v1, short int v2, short int v3)
{
	short int try = (v1+v2+v3)^((v1+v2+v3) << 16);
	while (try == v1 || try == v2 || try == v3)
		try++;
	return try;
}

/* We invalidate in as many ways as we can, so the OS rejects it */
static void invalidate_flock_struct(int signum)
{
	flock_struct.l_type = invalid_value(F_RDLCK, F_WRLCK, F_UNLCK);
	flock_struct.l_whence = invalid_value(SEEK_SET, SEEK_CUR, SEEK_END);
	flock_struct.l_start = -1;
	/* A large negative. */
	flock_struct.l_len = (((off_t)1 << (sizeof(off_t)*CHAR_BIT - 1)) + 1);
}

static int timeout_lock(int fd, int rw, off_t off, off_t len, bool waitflag,
			void *_timeout)
{
	int ret, saved_errno = errno;
	unsigned int timeout = *(unsigned int *)_timeout;

	flock_struct.l_type = rw;
	flock_struct.l_whence = SEEK_SET;
	flock_struct.l_start = off;
	flock_struct.l_len = len;

	CatchSignal(SIGALRM, invalidate_flock_struct);
	alarm(timeout);

	for (;;) {
		if (waitflag)
			ret = fcntl(fd, F_SETLKW, &flock_struct);
		else
			ret = fcntl(fd, F_SETLK, &flock_struct);

		if (ret == 0)
			break;

		/* Not signalled?  Something else went wrong. */
		if (flock_struct.l_len == len) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			saved_errno = errno;
			break;
		} else {
			saved_errno = EINTR;
			break;
		}
	}

	alarm(0);
	errno = saved_errno;
	return ret;
}

static int tdb_chainlock_with_timeout_internal(struct tdb_context *tdb,
					       TDB_DATA key,
					       unsigned int timeout,
					       int rw_type)
{
	union tdb_attribute locking;
	enum TDB_ERROR ecode;

	if (timeout) {
		locking.base.attr = TDB_ATTRIBUTE_FLOCK;
		ecode = tdb_get_attribute(tdb, &locking);
		if (ecode != TDB_SUCCESS)
			return ecode;

		/* Replace locking function with our own. */
		locking.flock.data = &timeout;
		locking.flock.lock = timeout_lock;

		ecode = tdb_set_attribute(tdb, &locking);
		if (ecode != TDB_SUCCESS)
			return ecode;
	}
	if (rw_type == F_RDLCK)
		ecode = tdb_chainlock_read(tdb, key);
	else
		ecode = tdb_chainlock(tdb, key);

	if (timeout) {
		tdb_unset_attribute(tdb, TDB_ATTRIBUTE_FLOCK);
	}
	return ecode;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	TDB_DATA key = tdb_mkdata("hello", 5);
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT,
			TDB_VERSION1, TDB_NOMMAP|TDB_VERSION1,
			TDB_CONVERT|TDB_VERSION1,
			TDB_NOMMAP|TDB_CONVERT|TDB_VERSION1 };
	struct agent *agent;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 15);

	agent = prepare_external_agent();

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		enum TDB_ERROR ecode;
		tdb = tdb_open("run-locktimeout.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &tap_log_attr);
		if (!ok1(tdb))
			break;

		/* Simple cases: should succeed. */
		ecode = tdb_chainlock_with_timeout_internal(tdb, key, 20,
							    F_RDLCK);
		ok1(ecode == TDB_SUCCESS);
		ok1(tap_log_messages == 0);

		tdb_chainunlock_read(tdb, key);
		ok1(tap_log_messages == 0);

		ecode = tdb_chainlock_with_timeout_internal(tdb, key, 20,
							    F_WRLCK);
		ok1(ecode == TDB_SUCCESS);
		ok1(tap_log_messages == 0);

		tdb_chainunlock(tdb, key);
		ok1(tap_log_messages == 0);

		/* OK, get agent to start transaction, then we should time out. */
		ok1(external_agent_operation(agent, OPEN, "run-locktimeout.tdb")
		    == SUCCESS);
		ok1(external_agent_operation(agent, TRANSACTION_START, "")
		    == SUCCESS);
		ecode = tdb_chainlock_with_timeout_internal(tdb, key, 20,
							    F_WRLCK);
		ok1(ecode == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);

		/* Even if we get a different signal, should be fine. */
		CatchSignal(SIGUSR1, do_nothing);
		external_agent_operation(agent, SEND_SIGNAL, "");
		ecode = tdb_chainlock_with_timeout_internal(tdb, key, 20,
							    F_WRLCK);
		ok1(ecode == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);

		ok1(external_agent_operation(agent, TRANSACTION_COMMIT, "")
		    == SUCCESS);
		ok1(external_agent_operation(agent, CLOSE, "")
		    == SUCCESS);
		tdb_close(tdb);
	}
	free_external_agent(agent);
	return exit_status();
}
