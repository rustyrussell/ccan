/* Regression test for audit finding F1 (2026-08-05): failtest clobbers
 * errno with an uninitialized value.
 *
 * failtest.c:add_history_() never initializes call->error, yet every
 * wrapper ends with `errno = p->error;`.  Consequences:
 *  (a) after a SUCCESSFUL wrapped call, errno is set to heap garbage;
 *  (b) after a GENUINE (non-injected) syscall failure, the real errno
 *      from the kernel is overwritten with heap garbage.
 *
 * Currently fails: test 2 sees garbage (deterministic under ASan,
 * 0xBEBEBEBE; deterministic here via heap poisoning), test 4 sees
 * garbage instead of EBADF.
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <unistd.h>
#include <ccan/failtest/failtest.c>
#include <stdio.h>
#include <ccan/tap/tap.h>

static enum failtest_result dont_fail(struct tlist_calls *history)
{
	return FAIL_DONT_FAIL;
}

int main(void)
{
	void *poison, *p;
	int fd;
	char buf[4];
	ssize_t r;

	alarm(10);

	plan_tests(4);
	failtest_init(0, NULL);
	failtest_hook = dont_fail;

	/* Poison the heap so the next history struct contains a known
	 * nonzero pattern (under ASan this happens anyway). */
	poison = malloc(sizeof(struct failtest_call));
	memset(poison, 0x5A, sizeof(struct failtest_call));
	free(poison);

	/* (a) successful malloc must not clobber errno. */
	errno = 0;
	p = failtest_malloc(10, "run-errno.c", 1);
	ok1(p != NULL);
	ok1(errno == 0);
	if (errno != 0)
		diag("errno after successful failtest_malloc = %d (garbage)",
		     errno);

	/* (b) genuine failure: reading an O_WRONLY fd fails with EBADF in
	 * the kernel; failtest must not replace that with garbage. */
	fd = open("/dev/null", O_WRONLY);
	if (fd < 0)
		abort();
	errno = 0;
	r = failtest_read(fd, buf, 1, "run-errno.c", 2);
	ok1(r == -1);
	ok1(errno == EBADF);
	if (errno != EBADF)
		diag("errno after genuinely failing failtest_read = %d, expected EBADF(%d)",
		     errno, EBADF);
	close(fd);

	/* Free p so the fixed module exits cleanly. */
	failtest_free(p);
	failtest_exit(exit_status());
}
