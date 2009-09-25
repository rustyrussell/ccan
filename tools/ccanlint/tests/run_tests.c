#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static void *do_run_tests(struct manifest *m)
{
	struct list_head *list = talloc(m, struct list_head);
	char *failures = talloc_strdup(m, "");
	struct ccan_file *i;

	list_head_init(list);

	run_tests.total_score = 0;
	list_for_each(&m->run_tests, i, list) {
		char *testout;
		run_tests.total_score++;
		/* FIXME: timeout here */
		testout = run_command(m, i->compiled);
		if (!testout)
			continue;
		failures = talloc_asprintf_append(failures,
						  "Running %s failed:\n",
						  i->name);
		failures = talloc_append_string(failures, testout);
	}

	list_for_each(&m->api_tests, i, list) {
		char *testout;
		run_tests.total_score++;
		/* FIXME: timeout here */
		testout = run_command(m, i->compiled);
		if (!testout)
			continue;
		failures = talloc_asprintf_append(failures,
						  "Running %s failed:\n",
						  i->name);
		failures = talloc_append_string(failures, testout);
	}

	if (streq(failures, "")) {
		talloc_free(failures);
		failures = NULL;
	}

	return failures;
}

static unsigned int score_run_tests(struct manifest *m, void *check_result)
{
	/* FIXME: be cleverer here */
	return 0;
}

static const char *describe_run_tests(struct manifest *m,
					  void *check_result)
{
	char *descrip = talloc_strdup(check_result, "Running tests failed:\n");

	return talloc_append_string(descrip, check_result);
}

struct ccanlint run_tests = {
	.name = "run and api tests run successfully",
	.total_score = 1,
	.score = score_run_tests,
	.check = do_run_tests,
	.describe = describe_run_tests,
	.can_run = can_run,
};

REGISTER_TEST(run_tests, &compile_tests, NULL);
