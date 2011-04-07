#include <ccan/tdb2/tdb.c>
#include <ccan/tdb2/open.c>
#include <ccan/tdb2/free.c>
#include <ccan/tdb2/lock.c>
#include <ccan/tdb2/io.c>
#include <ccan/tdb2/hash.c>
#include <ccan/tdb2/check.c>
#include <ccan/tdb2/transaction.c>
#include <ccan/tdb2/traverse.c>
#include <ccan/tap/tap.h>
#include "logging.h"

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

static int myunlock(int fd, int rw, off_t off, off_t len, void *_err)
{
	int *lock_err = _err;
	struct flock fl;
	int ret;

	if (*lock_err) {
		errno = *lock_err;
		return -1;
	}

	do {
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = off;
		fl.l_len = len;

		ret = fcntl(fd, F_SETLKW, &fl);
	} while (ret != 0 && errno == EINTR);

	return ret;
}

static int trav_err;
static int trav(struct tdb_context *tdb, TDB_DATA k, TDB_DATA d, int *err)
{
	*err = trav_err;
	return 0;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	struct tdb_context *tdb;
	int flags[] = { TDB_DEFAULT, TDB_NOMMAP,
			TDB_CONVERT, TDB_NOMMAP|TDB_CONVERT };
	union tdb_attribute lock_attr;
	struct tdb_data key = tdb_mkdata("key", 3);
	struct tdb_data data = tdb_mkdata("data", 4);
	int lock_err;

	lock_attr.base.attr = TDB_ATTRIBUTE_FLOCK;
	lock_attr.base.next = &tap_log_attr;
	lock_attr.flock.lock = mylock;
	lock_attr.flock.unlock = myunlock;
	lock_attr.flock.data = &lock_err;

	plan_tests(sizeof(flags) / sizeof(flags[0]) * 80);

	for (i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
		struct tdb_data d;

		/* Nonblocking open; expect no error message. */
		lock_err = EAGAIN;
		tdb = tdb_open("run-82-lockattr.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok(errno == lock_err, "Errno is %u", errno);
		ok1(!tdb);
		ok1(tap_log_messages == 0);

		lock_err = EINTR;
		tdb = tdb_open("run-82-lockattr.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok(errno == lock_err, "Errno is %u", errno);
		ok1(!tdb);
		ok1(tap_log_messages == 0);

		/* Forced fail open. */
		lock_err = ENOMEM;
		tdb = tdb_open("run-82-lockattr.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		ok1(errno == lock_err);
		ok1(!tdb);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = 0;
		tdb = tdb_open("run-82-lockattr.tdb", flags[i],
			       O_RDWR|O_CREAT|O_TRUNC, 0600, &lock_attr);
		if (!ok1(tdb))
			continue;
		ok1(tap_log_messages == 0);

		/* Nonblocking store. */
		lock_err = EAGAIN;
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking fetch. */
		lock_err = EAGAIN;
		ok1(!tdb_exists(tdb, key));
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(!tdb_exists(tdb, key));
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(!tdb_exists(tdb, key));
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(tdb_fetch(tdb, key, &d) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_fetch(tdb, key, &d) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_fetch(tdb, key, &d) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking delete. */
		lock_err = EAGAIN;
		ok1(tdb_delete(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_delete(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_delete(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking locks. */
		lock_err = EAGAIN;
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_chainlock(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(tdb_chainlock_read(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_chainlock_read(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_chainlock_read(tdb, key) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(tdb_lockall(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_lockall(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_lockall(tdb) == TDB_ERR_LOCK);
		/* This actually does divide and conquer. */
		ok1(tap_log_messages > 0);
		tap_log_messages = 0;

		lock_err = EAGAIN;
		ok1(tdb_lockall_read(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_lockall_read(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_lockall_read(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages > 0);
		tap_log_messages = 0;

		/* Nonblocking traverse; go nonblock partway through. */
		lock_err = 0;
		ok1(tdb_store(tdb, key, data, TDB_REPLACE) == 0);
		trav_err = EAGAIN;
		ok1(tdb_traverse(tdb, trav, &lock_err) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		trav_err = EINTR;
		lock_err = 0;
		ok1(tdb_traverse(tdb, trav, &lock_err) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		trav_err = ENOMEM;
		lock_err = 0;
		ok1(tdb_traverse(tdb, trav, &lock_err) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking transactions. */
		lock_err = EAGAIN;
		ok1(tdb_transaction_start(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = EINTR;
		ok1(tdb_transaction_start(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);
		lock_err = ENOMEM;
		ok1(tdb_transaction_start(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 1);
		tap_log_messages = 0;

		/* Nonblocking transaction prepare. */
		lock_err = 0;
		ok1(tdb_transaction_start(tdb) == 0);
		ok1(tdb_delete(tdb, key) == 0);

		lock_err = EAGAIN;
		ok1(tdb_transaction_prepare_commit(tdb) == TDB_ERR_LOCK);
		ok1(tap_log_messages == 0);

		lock_err = 0;
		ok1(tdb_transaction_prepare_commit(tdb) == 0);
		ok1(tdb_transaction_commit(tdb) == 0);

		/* And the transaction was committed, right? */
		ok1(!tdb_exists(tdb, key));
		tdb_close(tdb);
		ok1(tap_log_messages == 0);
	}
	return exit_status();
}
