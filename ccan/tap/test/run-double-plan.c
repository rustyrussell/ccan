/* Regression test (2026-08-05 audit, see audit-findings/tap.md F2):
 * plan_skip_all() must reject double planning like plan_tests() and
 * plan_no_plan() do, in both orders. */
#include <ccan/tap/tap.c>
#include <stdbool.h>
#include <sys/wait.h>

/* Child runs planner sequence with TAP output suppressed; exit 0 if
 * the second planner was wrongly accepted. */
static int child_status(bool skip_first)
{
	pid_t child = fork();
	int status;

	if (child == 0) {
		freopen("/dev/null", "w", stdout);
		freopen("/dev/null", "w", stderr);
		if (skip_first) {
			plan_skip_all("first");
			plan_tests(1);
		} else {
			plan_tests(1);
			plan_skip_all("second");
		}
		exit(0);
	}
	if (waitpid(child, &status, 0) != child)
		abort();
	return status;
}

int main(void)
{
	int status;

	plan_tests(2);

	status = child_status(false);
	ok1(WIFEXITED(status) && WEXITSTATUS(status) == 255);

	status = child_status(true);
	ok1(WIFEXITED(status) && WEXITSTATUS(status) == 255);

	return exit_status();
}
