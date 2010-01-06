/* FIXME: Do something tricky to ensure we really do loop in read_all. */

#include <ccan/read_write_all/read_write_all.h>
#include <ccan/read_write_all/read_write_all.c>
#include <ccan/tap/tap.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>

static volatile int sigcount;
static int p2c[2], c2p[2];
static void got_signal(int sig)
{
	char c = 0;
	if (write(p2c[1], &c, 1) == 1)
		sigcount++;
}

/* < PIPE_BUF *will* be atomic.  But > PIPE_BUF only *might* be non-atomic. */
#define BUFSZ (1024*1024)

int main(int argc, char *argv[])
{
	char buffer[BUFSZ*2] = { 0 };
	char c = 0;
	int status;
	pid_t child;

	plan_tests(6);

	/* We fork and torture parent. */
	if (pipe(p2c) != 0 || pipe(c2p) != 0)
		err(1, "pipe");
	child = fork();

	if (!child) {
		close(p2c[1]);
		close(c2p[0]);
		/* Child.  Make sure parent ready, then write in two parts. */
		if (read(p2c[0], &c, 1) != 1)
			exit(1);
		memset(buffer, 0xff, sizeof(buffer));
		if (!write_all(c2p[1], buffer, BUFSZ))
			exit(2);
		if (kill(getppid(), SIGUSR1) != 0)
			exit(3);
		/* Make sure they get signal. */
		if (read(p2c[0], &c, 1) != 1)
			exit(4);
		if (write(c2p[1], buffer, BUFSZ) != BUFSZ)
			exit(5);
		exit(0);
	}
	if (child == -1)
		err(1, "forking");

	close(p2c[0]);
	close(c2p[1]);
	signal(SIGUSR1, got_signal);
	ok1(write(p2c[1], &c, 1) == 1);
	ok1(read_all(c2p[0], buffer, sizeof(buffer)));
	ok1(memchr(buffer, 0, sizeof(buffer)) == NULL);
	ok1(sigcount == 1);
	ok1(wait(&status) == child);
	ok(WIFEXITED(status) && WEXITSTATUS(status) == 0,
	   "WIFEXITED(status) = %u, WEXITSTATUS(status) = %u",
	   WIFEXITED(status), WEXITSTATUS(status));
	return exit_status();
}
