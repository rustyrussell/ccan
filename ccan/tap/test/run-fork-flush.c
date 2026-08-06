/* Regression test for the fork()/buffering defect in ccan/tap.
 *
 * tap.3 ("BUGS" section) documents: "stdout is set to unbuffered mode
 * after calling any of the plan_* functions."  The code has that setbuf
 * commented out (tap.c:270), so when stdout is a pipe or file (i.e.
 * under every test harness) tap output is fully buffered.  A forked
 * child that never calls any tap function but exit()s normally flushes
 * its inherited copy of the buffer: every TAP line printed before the
 * fork appears twice, and the plan line appears twice, which prove(1)
 * rejects ("More than one plan found in TAP output").
 *
 * Like test/run.c, we redirect stdout to a pipe and check the exact
 * bytes; to the outside this looks like one big test.  Pre-fix this
 * fails: the child's flush inserts a duplicate "1..2\nok 1 - true\n".
 *
 * No production files are modified.  alarm()-bounded.
 */
#include <ccan/tap/tap.h>
#include <ccan/tap/tap.c>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

/* write_all inlined here to avoid circular dependency (as in run.c). */
static void write_all(int fd, const void *data, size_t size)
{
	while (size) {
		ssize_t done;

		done = write(fd, data, size);
		if (done <= 0)
			_exit(1);
		data = (const char *)data + done;
		size -= done;
	}
}

int main(void)
{
	int p[2];
	int stdoutfd;
	pid_t pid;
	char buffer[PIPE_BUF+1];
	ssize_t r;
	static const char expected[] = "1..2\nok 1 - true\nok 2 - true\n";

	alarm(10);

	/* Our own result is reported on the saved stdout fd. */
	stdoutfd = dup(STDOUT_FILENO);
	if (stdoutfd < 0)
		err(1, "dup of stdout failed");

	write_all(stdoutfd, "1..1\n", strlen("1..1\n"));

	if (pipe(p) != 0)
		err(1, "pipe failed");

	/* tap output now goes to the pipe; stdout stays fully buffered
	 * (default for a pipe) — exactly what tap.3 says must not happen. */
	if (dup2(p[1], STDOUT_FILENO) < 0)
		err(1, "dup2 failed");
	close(p[1]);

	plan_tests(2);
	ok1(true);

	pid = fork();
	if (pid < 0)
		err(1, "fork failed");
	if (pid == 0)
		exit(0);	/* child: no tap calls, plain exit() */

	if (waitpid(pid, NULL, 0) != pid)
		err(1, "waitpid failed");

	ok1(true);

	if (exit_status() != 0)
		err(1, "exit_status() != 0");

	/* Now flush our own buffer and compare the complete stream. */
	fflush(stdout);
	r = read(p[0], buffer, sizeof(buffer)-1);
	if (r < 0)
		err(1, "reading from pipe");
	buffer[r] = '\0';

	if (strcmp(buffer, expected) != 0) {
		write_all(stdoutfd, "not ok 1 - TAP stream duplicated by forked child: ",
			  strlen("not ok 1 - TAP stream duplicated by forked child: "));
		write_all(stdoutfd, buffer, strlen(buffer));
		write_all(stdoutfd, "\n", 1);
		exit(1);
	}

	write_all(stdoutfd, "ok 1 - forked child's exit() adds nothing to the TAP stream\n",
		  strlen("ok 1 - forked child's exit() adds nothing to the TAP stream\n"));
	exit(0);
}
