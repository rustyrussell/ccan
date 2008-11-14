/* FIXME: Do something tricky to ensure we really do loop in write_all. */

#include "read_write_all/read_write_all.h"
#include "read_write_all/read_write_all.c"
#include "tap/tap.h"
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <limits.h>
#include <sys/wait.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static volatile int sigcount;
static void got_signal(int sig)
{
	sigcount++;
}

/* < PIPE_BUF *will* be atomic.  But > PIPE_BUF only *might* be non-atomic. */
#define BUFSZ (1024*1024)

int main(int argc, char *argv[])
{
	char buffer[BUFSZ*2] = { 0 };
	int p2c[2];
	int status;
	pid_t child;

	plan_tests(4);

	/* We fork and torture parent. */
	if (pipe(p2c) != 0)
		err(1, "pipe");
	child = fork();

	if (!child) {
		/* Make sure they started writing. */
		if (read(p2c[0], buffer, 1) != 1)
			exit(1);
		if (kill(getppid(), SIGUSR1) != 0)
			exit(2);
		if (!read_all(p2c[0], buffer+1, sizeof(buffer)-1))
			exit(3);
		if (memchr(buffer, 0, sizeof(buffer))) {
			fprintf(stderr, "buffer has 0 at offset %i\n",
				memchr(buffer, 0, sizeof(buffer)) - (void *)buffer);
			exit(4);
		}
		exit(0);
	}
	if (child == -1)
		err(1, "forking");

	memset(buffer, 0xff, sizeof(buffer));
	signal(SIGUSR1, got_signal);
	ok1(write_all(p2c[1], buffer, sizeof(buffer)));
	ok1(sigcount == 1);
	ok1(wait(&status) == child);
	ok(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	   "WIFEXITED(status) = %u, WEXITSTATUS(status) = %u",
	   WIFEXITED(status), WEXITSTATUS(status));
	return exit_status();
}
