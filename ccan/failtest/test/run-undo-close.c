/* Regression test for audit finding F7 (2026-08-05): failtest_undo.h's
 * close macro has the wrong arity.
 *
 * failtest_undo.h:43: `#define close(fd) failtest_close(fd)` — but
 * failtest_close() takes three arguments (failtest_proto.h:30), so any
 * user that calls close() after including failtest_undo.h fails to
 * compile:
 *   error: too few arguments to function call, expected 3, have 1
 *
 * Currently fails to COMPILE (by design, proving the defect).
 *
 * Temporary auditor-added test; no production files modified.
 */
/* Include the implementation first, before the override macros. */
#include <ccan/failtest/failtest.c>
#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <ccan/failtest/failtest_undo.h>
#include <ccan/tap/tap.h>

int main(void)
{
	int fd;

	plan_tests(2);

	/* After failtest_undo.h these route through the non-failing
	 * wrappers (NULL file suppresses failure injection). */
	fd = open("/dev/null", O_RDONLY);
	ok1(fd >= 0);
	ok1(close(fd) == 0);

	return exit_status();
}
