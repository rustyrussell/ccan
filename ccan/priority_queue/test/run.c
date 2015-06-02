#include <ccan/priority_queue/priority_queue.h>
#include <ccan/tap/tap.h>

int main(void)
{
	/* This is how many tests you plan to run */
	plan_tests(3);

	/* Simple thing we expect to succeed */
	ok1(some_test())
	/* Same, with an explicit description of the test. */
	ok(some_test(), "%s with no args should return 1", "some_test")
	/* How to print out messages for debugging. */
	diag("Address of some_test is %p", &some_test)
	/* Conditional tests must be explicitly skipped. */
#if HAVE_SOME_FEATURE
	ok1(test_some_feature())
#else
	skip(1, "Don't have SOME_FEATURE")
#endif

	/* This exits depending on whether all tests passed */
	return exit_status();
}
