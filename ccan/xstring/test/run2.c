#include <ccan/failtest/failtest_override.h>
#include <ccan/failtest/failtest.h>
#include <ccan/xstring/xstring.h>
/* Include the C files directly. */
#include <ccan/xstring/xstring.c>
#include <ccan/tap/tap.h>

unsigned last_fail_line;

static enum failtest_result once_only(struct tlist_calls *history)
{
	const struct failtest_call *tail = tlist_tail(history, list);

	if (tail->line == last_fail_line)
		return FAIL_DONT_FAIL;

	last_fail_line = tail->line;
	return FAIL_OK;
}

int main(int argc, char *argv[])
{
	failtest_init(argc, argv);
	failtest_hook = once_only;
	plan_tests(3);

	xstring *x;

	ok1((x = xstrNew(100)) != NULL); // fail first malloc
	if (x) xstrFree(x);

	ok1((x = xstrNew(100)) != NULL); // fail second malloc
	if (x) xstrFree(x);

	ok1((x = xstrNew(0)) == NULL && errno == EINVAL);

	failtest_exit(exit_status());
}
