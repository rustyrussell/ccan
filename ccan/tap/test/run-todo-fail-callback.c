/* Regression test (2026-08-05 audit, see audit-findings/tap.md F3):
 * tap_fail_callback must not fire for failing TODO tests (their
 * failures are deliberately not counted); it must still fire for a
 * real failure. */
#include <ccan/tap/tap.c>
#include <stdbool.h>
#include <sys/wait.h>

static unsigned int cb_count;

static void count_fail(void)
{
	cb_count++;
}

/* Child runs the scenario with TAP output suppressed; exits with the
 * observed callback count. */
static int child_cb_count(bool in_todo)
{
	pid_t child = fork();
	int status;

	if (child == 0) {
		freopen("/dev/null", "w", stdout);
		tap_fail_callback = count_fail;
		/* Plan state is inherited from the parent. */
		if (in_todo) {
			todo_start("expected");
			ok1(0);
			todo_end();
		} else {
			ok1(0);
		}
		exit(cb_count);
	}
	if (waitpid(child, &status, 0) != child)
		abort();
	return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int main(void)
{
	plan_tests(2);

	ok1(child_cb_count(true) == 0);
	ok1(child_cb_count(false) == 1);

	return exit_status();
}
