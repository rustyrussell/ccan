/* Regression test for audit finding F6 (2026-08-05): off_max() returns
 * a wrong maximum in BOTH branches.
 *
 * failtest.c:372: `return (off_t)0x7FFFFFF;` is 0x07FFFFFF
 * (134217727 = 2^27-1), not 0x7FFFFFFF (2147483647 = 2^31-1).
 * failtest.c:374: `return (off_t)0x7FFFFFFFFFFFFFFULL;` is
 * 0x07FFFFFFFFFFFFFF (576460752303423487 = 2^59-1), not
 * 0x7FFFFFFFFFFFFFFF (9223372036854775807 = 2^63-1).
 *
 * This makes the fcntl lock bookkeeping treat locks "to end of file"
 * (l_len == 0, see end_of() at failtest.c:1569) as ending at byte
 * 2^27-1 / 2^59-1; a tracked range whose end coincides with the wrong
 * maximum is also mis-translated back to l_len=0 ("to real EOF") by
 * get_locks() (failtest.c:397-398).
 *
 * Currently fails on both LP64 and ILP32.
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <ccan/failtest/failtest.c>
#include <ccan/tap/tap.h>

int main(void)
{
	plan_tests(1);
	failtest_init(0, NULL);

	if (sizeof(off_t) == 4)
		ok1(off_max() == (off_t)0x7FFFFFFF);
	else
		ok1(off_max() == (off_t)0x7FFFFFFFFFFFFFFFLL);

	return exit_status();
}
