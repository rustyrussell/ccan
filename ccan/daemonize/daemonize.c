#include <ccan/daemonize/daemonize.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

/* This code is based on Stevens Advanced Programming in the UNIX
 * Environment. */
bool daemonize(void)
{
	pid_t pid;

	/* Separate from our parent via fork, so init inherits us. */
	if ((pid = fork()) < 0)
		return false;
	if (pid != 0)
		exit(0);

	/* Don't hold files open. */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	/* Session leader so ^C doesn't whack us. */
	setsid();
	/* Move off any mount points we might be in. */
	chdir("/");
	/* Discard our parent's old-fashioned umask prejudices. */
	umask(0);
	return true;
}
