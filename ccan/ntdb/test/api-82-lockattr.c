#include "../private.h" // for ntdb_fcntl_unlock
#include "../ntdb.h"
#include "tap-interface.h"
#include <errno.h>
#include "logging.h"
#include "helpapi-external-agent.h"

static int mylock(int fd, int rw, off_t off, off_t len, bool waitflag,
		  void *_err)
{
	int *lock_err = _err;
	struct flock fl;
	int ret;

	if (*lock_err) {
		errno = *lock_err;
		return -1;
	}

	do {
		fl.l_type = rw;
		fl.l_whence = SEEK_SET;
		fl.l_start = off;
		fl.l_len = len;

		if (waitflag)
			ret = fcntl(fd, F_SETLKW, &fl);
		else
			ret = fcntl(fd, F_SETLK, &fl);
	} while (ret != 0 && errno == EINTR);

	return ret;
}

static int trav_err;
static int trav(struct ntdb_context *ntdb, NTDB_DATA k, NTDB_DATA d, int *terr)
{
	*terr = trav_err;
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ntdb_context *ntdb;
	int flags[] = { NTDB_DEFAULT, NTDB_NOMMAP,
			NTDB_CONVERT, NTDB_NOMMAP|NTDB_CONVERT };
	union ntdb_attribute lock_attr;
	NTDB_DATA key = ntdb_mkdata("key", 3);
	NTDB_DATA data = ntdb_mkdata("data", 4);
	int lock_err;

	lock_attr.base.attr = NTDB_ATTRIBUTE_FLOCK;
	lock_attr.base.next = &tap_log_attr;
	lock_attr.flock.lock = mylock;
	lock_attr.flock.unlock = ntdb_fcntl_unlock;
	lock_attr.flock.data = &lock_err;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 81);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		NTDB_DATA d;

		/* Nonblocking open; expect no error message. */
		lock_err = EAGAIN;
		ntdb = ntdb_open("run-82-lockattr.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok(errno == lock_err, "Errno is %u", errno);
		ok1(!ntdb);
		ok1(tap_log_messages == 0);

		lock_err = EINTR;
		ntdb = ntdb_open("run-82-lockattr.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok(errno == lock_err, "Errno is %u", errno);
		ok1(!ntdb);
		ok1(tap_log_messages == 0);

		/* Forced fail open. */
		lock_err = ENOMEM;
		ntdb = ntdb_open("run-82-lockattr.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok1(errno == lock_err);
		ok1(!ntdb);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = 0;
		ntdb = ntdb_open("run-82-lockattr.ntdb", flags[i]|MAYBE_NOSYNC,
				 O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		if (!ok1(ntdb))
			continue;
		ok1(tap_log_messages == 0);

		/* Nonblocking store. */
		lock_err = EAGAIN;
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking fetch. */
		lock_err = EAGAIN;
		ok1(!ntdb_exists(ntdb, key));
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(!ntdb_exists(ntdb, key));
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(!ntdb_exists(ntdb, key));
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_fetch(ntdb, key, &d) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking delete. */
		lock_err = EAGAIN;
		ok1(ntdb_delete(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_delete(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_delete(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking locks. */
		lock_err = EAGAIN;
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_chainlock(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(ntdb_chainlock_read(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_chainlock_read(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_chainlock_read(ntdb, key) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(ntdb_lockall(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_lockall(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_lockall(ntdb) == NTDB_ERR_LOCK);
		/* This actually does divide and conquer. */
		ok1(tap_log_messages > 0);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(ntdb_lockall_read(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_lockall_read(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_lockall_read(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages > 0);
		tap_log_messages = 0;

		/* Nonblocking traverse; go nonblock partway through. */
		lock_err = 0;
		ok1(ntdb_store(ntdb, key, data, NTDB_REPLACE) == 0);
		/* Need two entries to ensure two lock attempts! */
		ok1(ntdb_store(ntdb, ntdb_mkdata("key2", 4), data,
			       NTDB_REPLACE) == 0);
		trav_err = EAGAIN;
		ok1(ntdb_traverse(ntdb, trav, &lock_err) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		trav_err = EINTR;
		lock_err = 0;
		ok1(ntdb_traverse(ntdb, trav, &lock_err) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		trav_err = ENOMEM;
		lock_err = 0;
		ok1(ntdb_traverse(ntdb, trav, &lock_err) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking transactions. */
		lock_err = EAGAIN;
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(ntdb_transaction_start(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking transaction prepare. */
		lock_err = 0;
		ok1(ntdb_transaction_start(ntdb) == 0);
		ok1(ntdb_delete(ntdb, key) == 0);

		lock_err = EAGAIN;
		ok1(ntdb_transaction_prepare_commit(ntdb) == NTDB_ERR_LOCK);
		ok1(tap_log_messages == 0);

		lock_err = 0;
		ok1(ntdb_transaction_prepare_commit(ntdb) == 0);
		ok1(ntdb_transaction_commit(ntdb) == 0);

		/* And the transaction was committed, right? */
		ok1(!ntdb_exists(ntdb, key));
		ntdb_close(ntdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
