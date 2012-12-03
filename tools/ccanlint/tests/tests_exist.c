#include <tools/ccanlint/ccanlint.h>
#include <ccan/tal/str/str.h>
#include <ccan/tal/path/path.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>

static void check_tests_exist(struct manifest *m,
			      unsigned int *timeleft, struct score *score);

static struct ccanlint tests_exist = {
	.key = "tests_exist",
	.name = "Module has test directory with tests in it",
	.check = check_tests_exist,
	.needs = "info_exists"
};
REGISTER_TEST(tests_exist);

static void handle_no_tests(struct manifest *m, struct score *score)
{
	FILE *run;
	struct ccan_file *i;
	char *test_dir = tal_fmt(m, "%s/test", m->dir), *run_file;

	printf(
	"CCAN modules have a directory called test/ which contains tests.\n"
	"There are four kinds of tests: api, run, compile_ok and compile_fail:\n"
	"you can tell which type of test a C file is by its name, eg 'run.c'\n"
	"and 'run-simple.c' are both run tests.\n\n"

	"The simplest kind of test is a run test, which must compile with no\n"
	"warnings, and then run: it is expected to use ccan/tap to report its\n"
	"results in a simple and portable format.  It should #include the C\n"
	"files from the module directly (so it can probe the internals): the\n"
	"module will not be linked in.  The test will be run in a temporary\n"
	"directory, with the test directory symlinked under test/.\n\n"

	"api tests are just like a run test, except it is a guarantee of API\n"
	"stability: this test should pass on all future versions of the\n"
	"module.  They *are* linked to the module, since they should only\n"
	"test the API, not the internal state.\n\n"

	"compile_ok tests are a subset of run tests: they must compile and\n"
	"link, but aren't run.\n\n"

	"compile_fail tests are tests which should fail to compile (or emit\n"
	"warnings) or link when FAIL is defined, but should compile and link\n"
	"when it's not defined: this helps ensure unrelated errors don't make\n"
	"compilation fail.\n\n"

	"Note that only API tests are linked against the files in the module!\n"
		);

	if (!ask("Should I create a template test/run.c file for you?"))
		return;

	if (mkdir(test_dir, 0700) != 0) {
		if (errno != EEXIST)
			err(1, "Creating test/ directory");
	}

	run_file = tal_fmt(test_dir, "%s/run.c", test_dir);
	run = fopen(run_file, "w");
	if (!run)
		err(1, "Trying to create a test/run.c");

	fprintf(run, "#include <ccan/%s/%s.h>\n", m->modname, m->basename);
	if (!list_empty(&m->c_files)) {
		fputs("/* Include the C files directly. */\n", run);
		list_for_each(&m->c_files, i, list)
			fprintf(run, "#include <ccan/%s/%s>\n",
				m->modname, i->name);
	}
	fprintf(run, "%s",
		"#include <ccan/tap/tap.h>\n\n"
		"int main(void)\n"
		"{\n"
		"	/* This is how many tests you plan to run */\n"
		"	plan_tests(3);\n"
		"\n"
		"	/* Simple thing we expect to succeed */\n"
		"	ok1(some_test())\n"
		"	/* Same, with an explicit description of the test. */\n"
		"	ok(some_test(), \"%s with no args should return 1\", \"some_test\")\n"
		"	/* How to print out messages for debugging. */\n"
		"	diag(\"Address of some_test is %p\", &some_test)\n"
		"	/* Conditional tests must be explicitly skipped. */\n"
		"#if HAVE_SOME_FEATURE\n"
		"	ok1(test_some_feature())\n"
		"#else\n"
		"	skip(1, \"Don\'t have SOME_FEATURE\")\n"
		"#endif\n"
		"\n"
		"	/* This exits depending on whether all tests passed */\n"
		"	return exit_status();\n"
		"}\n");
	fclose(run);
}

static void check_tests_exist(struct manifest *m,
			    unsigned int *timeleft, struct score *score)
{
	struct stat st;
	char *test_dir = path_join(m, m->dir, "test");

	if (lstat(test_dir, &st) != 0) {
		score->error = tal_strdup(score, "No test directory");
		if (errno != ENOENT)
			err(1, "statting %s", test_dir);
		tests_exist.handle = handle_no_tests;
		/* We "pass" this. */
		score->pass = true;
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		score->error = tal_strdup(score, "test is not a directory");
		return;
	}

	if (list_empty(&m->api_tests)
	    && list_empty(&m->run_tests)
	    && list_empty(&m->compile_ok_tests)
	    && list_empty(&m->compile_fail_tests)) {
		score->error = tal_strdup(score, "No tests in test directory");
		tests_exist.handle = handle_no_tests;
		return;
	}
	score->pass = true;
	score->score = score->total;
}
