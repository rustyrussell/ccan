/* Regression test for audit finding F3 (2026-08-05):
 * failtest_fcntl(F_GETLK) never copies the result back to the caller.
 *
 * failtest.c:1599-1605 copies the caller's struct flock INTO the
 * history record and calls the real fcntl() on the copy; the kernel's
 * F_GETLK result (conflicting-lock info, or l_type == F_UNLCK) is
 * written to the copy and discarded.
 *
 * Currently fails: test 2 (l_type stays F_WRLCK instead of F_UNLCK).
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <ccan/failtest/failtest.c>
#include <stdio.h>
#include <ccan/tap/tap.h>

static enum failtest_result dont_fail(struct tlist_calls *history)
{
	return FAIL_DONT_FAIL;
}

int main(void)
{
	int fd;
	struct flock fl;

	alarm(10);

	plan_tests(3);
	failtest_init(0, NULL);
	failtest_hook = dont_fail;

	fd = open("run-getlk-scratch", O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd < 0)
		abort();

	/* No locks held: F_GETLK must report l_type == F_UNLCK. */
	memset(&fl, 0, sizeof(fl));
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = 0;
	fl.l_len = 1;
	ok1(failtest_fcntl(fd, "run-getlk.c", 1, F_GETLK, &fl) == 0);
	ok1(fl.l_type == F_UNLCK);
	if (fl.l_type != F_UNLCK)
		diag("l_type = %d after F_GETLK on unlocked file; caller's flock never updated",
		     fl.l_type);

	/* Sanity: direct fcntl does update it. */
	fl.l_type = F_WRLCK;
	ok1(fcntl(fd, F_GETLK, &fl) == 0 && fl.l_type == F_UNLCK);

	close(fd);
	failtest_exit(exit_status());
}
