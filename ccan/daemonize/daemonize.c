/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/daemonize/daemonize.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* This code is based on Stevens Advanced Programming in the UNIX
 * Environment. */
bool daemonize(void)
{
        struct sigaction old_sa, sa;
	const char *dev_null = "/dev/null";
	pid_t pid;
	int fd;
	int sa_rc;

	/* SIGHUP may be thrown when the parent exits below.
	   So, set to ignore it and save previous action.
	 */
        sigemptyset(&sa.sa_mask);
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	sa_rc = sigaction(SIGHUP, &sa, &old_sa);

	/* Separate from our parent via fork, so init inherits us. */
	if ((pid = fork()) < 0)
		return false;
	/* use _exit() to avoid triggering atexit() processing */
	if (pid != 0)
		_exit(0);

	/* Restore previous SIGHUP action. */
        if (sa_rc != -1)
	        sigaction(SIGHUP, &old_sa, NULL);

	/* Don't hold files open. */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Many routines write to stderr; that can cause chaos if used
	 * for something else, so set it here. */
	/* reopen STDIN */
	if ((fd = open(dev_null, O_RDONLY)) == -1)
		return false;
	if (dup2(fd, STDIN_FILENO) != STDIN_FILENO)
	        return false;
	if (fd > STDERR_FILENO) close(fd);
	/* reopen STDOUT */
	if ((fd = open("/dev/null", O_WRONLY)) == -1)
		return false;
	if (dup2(fd, STDOUT_FILENO) != STDOUT_FILENO)
	        return false;
	/* reopen STDERR */
	if (dup2(fd, STDERR_FILENO) != STDERR_FILENO)
		return false;
	if (fd > STDERR_FILENO) close(fd);

	/* Session leader so ^C doesn't whack us. */
	if (setsid() == (pid_t)-1)
	        return false;
	/* Move off any mount points we might be in. */
	if (chdir("/") != 0)
		return false;

	/* Discard our parent's old-fashioned umask prejudices. */
	umask(0);
	return true;
}
