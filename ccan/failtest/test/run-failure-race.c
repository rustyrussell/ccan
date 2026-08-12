/* Regression test for audit finding F4 (2026-08-05): a failing child's
 * failure report races with the parent and is lost; the child is killed
 * by SIGPIPE and the parent reports "Killed by signal 13" instead.
 *
 * failtest.c:591-634 failtest_cleanup(): on a leak the child prints
 * "Leak at ..." to its stdout with printf() — fully buffered, since the
 * child's stdout is a pipe to the parent — then reports FAILURE with a
 * direct write_all() on the control fd (failtest.c:632); only exit()
 * flushes stdout afterwards.  The parent's poll loop stops the moment
 * it reads FAILURE (failtest.c:860) and closes the output pipe read end
 * (failtest.c:862), so the child's exit-flush hits EPIPE/SIGPIPE; the
 * parent then reports "Killed by signal 13" (failtest.c:865-871) and
 * the real diagnostic never appears.
 *
 * The atexit() handler in the child merely widens the race window to
 * make this deterministic (atexit handlers run before stdio flushing);
 * the race itself is entirely inside failtest and was also observed
 * unforced in a nested-fork scenario (see audit-findings/failtest.md).
 *
 * Self-checking via re-exec: the outer process runs the leaky scenario
 * with output captured to a temp file and checks what was reported.
 * The scenario always exits nonzero (a leak *is* a test failure); what
 * matters is *how* it is reported.
 *
 * Currently fails: "Leak at" missing, "Killed by signal 13" present.
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <unistd.h>
#include <ccan/failtest/failtest.c>
#include <stdio.h>
#include <sys/wait.h>
#include <ccan/tap/tap.h>

static void widen_race_window(void)
{
	/* Only the failing child (the one failtest forked) delays. */
	if (failtest_has_failed())
		usleep(100000);
}

static void leaky_scenario(int argc, char *argv[])
{
	void *p, *q;

	plan_tests(1);
	failtest_init(argc, argv);
	atexit(widen_race_window);

	p = failtest_malloc(10, "run-failure-race.c", 1);
	if (!p) {
		/* malloc-failure child: allocate and leak something. */
		q = failtest_malloc(20, "run-failure-race.c", 2);
		if (!q)
			failtest_exit(0);	/* grandchild, no leak */
		/* Deliberately leak q. */
		failtest_exit(0);
	}
	free(p);

	ok1(1);
	failtest_exit(exit_status());
}

int main(int argc, char *argv[])
{
	char tmpl[] = "/tmp/run-failure-race-XXXXXX";
	char buf[65536];
	ssize_t len;
	int fd, status;
	pid_t child;

	if (argc > 1) {
		leaky_scenario(argc, argv);
		abort();
	}

	alarm(30);
	plan_tests(2);

	fd = mkstemp(tmpl);
	if (fd < 0)
		abort();

	child = fork();
	if (child == 0) {
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		/* argv[0] works under valgrind; /proc/self/exe does not. */
		execl(argv[0], argv[0], "inner", NULL);
		execl("/proc/self/exe", "/proc/self/exe", "inner", NULL);
		abort();
	}
	if (child < 0 || waitpid(child, &status, 0) != child)
		abort();

	len = pread(fd, buf, sizeof(buf) - 1, 0);
	if (len < 0)
		abort();
	buf[len] = '\0';
	close(fd);
	unlink(tmpl);

	/* The child had a leak: nonzero exit is correct.  But the leak
	 * must be *reported*, and no child may die of SIGPIPE. */
	ok1(strstr(buf, "Leak at run-failure-race.c:2") != NULL);
	ok1(strstr(buf, "Killed by signal") == NULL);
	if (!strstr(buf, "Leak at"))
		diag("captured output: %s", buf);

	return exit_status();
}
