/* Regression test for audit finding F2 (2026-08-05): failtest_pipe()
 * mishandles a GENUINE pipe() failure.
 *
 * failtest.c:1213-1215 records the real pipe() result without checking
 * it.  If pipe() fails (fd exhaustion), the history entry keeps
 * can_leak=true with cleanup_pipe set, so failtest_exit() close()es the
 * UNINITIALIZED fds[] and reports a spurious "Leak", forcing exit
 * status 1 even though the caller handled the failure.  errno is also
 * garbage (finding F1).
 *
 * Currently fails: test 2 (errno garbage), plus a spurious
 * "Leak at ..." line and exit status 1.
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <ccan/failtest/failtest.c>
#include <stdio.h>
#include <sys/resource.h>
#include <ccan/tap/tap.h>

static enum failtest_result dont_fail(struct tlist_calls *history)
{
	return FAIL_DONT_FAIL;
}

int main(void)
{
	struct rlimit lim;
	int fds[2];
	int devnull[128];
	unsigned int i, n = 0;
	int r;

	alarm(10);

	plan_tests(2);
	failtest_init(0, NULL);
	failtest_hook = dont_fail;

	/* Squeeze the fd limit (after init), then exhaust it so the real
	 * pipe() fails with EMFILE. */
	if (getrlimit(RLIMIT_NOFILE, &lim) != 0)
		abort();
	if (lim.rlim_cur > 64) {
		lim.rlim_cur = 64;
		if (setrlimit(RLIMIT_NOFILE, &lim) != 0)
			abort();
	}
	for (i = 0; i < 128; i++) {
		devnull[i] = open("/dev/null", O_RDONLY);
		if (devnull[i] < 0)
			break;
		n++;
	}

	errno = 0;
	r = failtest_pipe(fds, "run-pipe-real-fail.c", 1);
	ok1(r == -1);
	ok1(errno == EMFILE);
	if (errno != EMFILE)
		diag("errno after genuine pipe() failure = %d, expected EMFILE(%d)",
		     errno, EMFILE);

	for (i = 0; i < n; i++)
		close(devnull[i]);

	/* Pre-fix this also prints a spurious "Leak at ..." and forces
	 * status 1. */
	failtest_exit(exit_status());
}
