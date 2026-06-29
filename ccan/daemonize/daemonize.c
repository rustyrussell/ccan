/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/daemonize/daemonize.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

/* This code is based on Stevens' Advanced Programming in the UNIX
 * Environment. */
bool daemonize(void)
{
	pid_t pid;
	int err;
	int errfd[2];

	/* Create a pipe to wait for child's setsid */
	if (pipe(errfd) != 0)
		return false;

	/* Separate from our parent via fork, so init inherits us. */
	if ((pid = fork()) < 0) {
		err = errno;
		close(errfd[0]);
		close(errfd[1]);
		errno = err;
		return false;
	}

	if (pid != 0) {
		/* Parent: wait for child to setsid, so it doesn't get
		 * SIGHUP! */
		close(errfd[1]);
	again:
		if (read(errfd[0], &err, sizeof(err)) != sizeof(err)) {
			if (errno == EINTR)
				goto again;
			err = EIO;
		} else if (err == 0) {
			/* use _exit() to avoid triggering atexit() processing */
			_exit(0);
		}

		close(errfd[0]);
		errno = err;
		return false;
	}

	/* Don't hold files open. */
	close(errfd[0]);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Many routines write to stderr; that can cause chaos if used
	 * for something else, so set it here. */
	if (open("/dev/null", O_WRONLY) != 0)
		goto child_fail;
	if (dup2(0, STDERR_FILENO) != STDERR_FILENO)
		goto child_fail;
	close(0);

	/* Session leader so ^C doesn't whack us. */
	if (setsid() == (pid_t)-1)
		goto child_fail;

	/* Move off any mount points we might be in. */
	if (chdir("/") != 0)
		goto child_fail;

	/* Discard our parent's old-fashioned umask prejudices. */
	umask(0);
	err = 0;
	/* If parent goes away early, that's not really our problem. */
	if (write(errfd[1], &err, sizeof(err)) != sizeof(err))
		;
	close(errfd[1]);
	return true;

child_fail:
	/* Parent will get errno, we will exit quietly */
	err = errno;
	if (write(errfd[1], &err, sizeof(err)) != sizeof(err))
		;
	_exit(err);
}
