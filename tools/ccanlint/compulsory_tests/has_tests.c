#include <tools/ccanlint/ccanlint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <ccan/talloc/talloc.h>

static char test_is_not_dir[] = "test is not a directory";

static void *check_has_tests(struct manifest *m, unsigned int *timeleft)
{
	struct stat st;
	char *test_dir = talloc_asprintf(m, "%s/test", m->dir);

	if (lstat(test_dir, &st) != 0) {
		if (errno != ENOENT)
			err(1, "statting %s", test_dir);
		return "You have no test directory";
	}

	if (!S_ISDIR(st.st_mode))
		return test_is_not_dir;

	if (list_empty(&m->api_tests)
	    && list_empty(&m->run_tests)
	    && list_empty(&m->compile_ok_tests)) {
		if (list_empty(&m->compile_fail_tests)) 
			return "You have no tests in the test directory";
		else
			return "You have no positive tests in the test directory";
	}
	return NULL;
}

static const char *describe_has_tests(struct manifest *m, void *check_result)
{
	return talloc_asprintf(m, "%s\n\n"
        "CCAN modules have a directory called test/ which contains tests.\n"
	"There are four kinds of tests: api, run, compile_ok and compile_fail:\n"
	"you can tell which type of test a C file is by its name, eg 'run.c'\n"
	"and 'run-simple.c' are both run tests.\n\n"

	"The simplest kind of test is a run test, which must compile with no\n"
	"warnings, and then run: it is expected to use libtap to report its\n"
	"results in a simple and portable format.  It should #include the C\n"
	"files from the module directly (so it can probe the internals): the\n"
	"module will not be linked in.\n\n"

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

	"Note that the tests are not linked against the files in the\n"
	"above: you should directly #include those C files you want.  This\n"
	"allows access to static functions and use special effects inside\n"
	"test files\n", (char *)check_result);
}

static void handle_no_tests(struct manifest *m, void *check_result)
{
	FILE *run;
	struct ccan_file *i;

	if (check_result == test_is_not_dir)
		return;

	if (!ask("Should I create a template test/run.c file for you?"))
		return;

	if (mkdir("test", 0700) != 0) {
		if (errno != EEXIST)
			err(1, "Creating test/ directory");
	}

	run = fopen("test/run.c", "w");
	if (!run)
		err(1, "Trying to create a test/run.c");

	fputs("/* Include the main header first, to test it works */\n", run);
	fprintf(run, "#include \"%s/%s.h\"\n", m->basename, m->basename);
	fputs("/* Include the C files directly. */\n", run);
	list_for_each(&m->c_files, i, list)
		fprintf(run, "#include \"%s/%s\"\n", m->basename, i->name);
	fputs("#include \"tap/tap.h\"\n", run);
	fputs("\n", run);

	fputs("int main(void)\n", run);
	fputs("{\n", run);
	fputs("\t/* This is how many tests you plan to run */\n", run);
	fputs("\tplan_tests(3);\n", run);
	fputs("\n", run);
	fputs("\t/* Simple thing we expect to succeed */\n", run);
	fputs("\tok1(some_test())\n", run);
	fputs("\t/* Same, with an explicit description of the test. */\n", run);
	fputs("\tok(some_test(), \"%s with no args should return 1\", \"some_test\")\n", run);
	fputs("\t/* How to print out messages for debugging. */\n", run);
	fputs("\tdiag(\"Address of some_test is %p\", &some_test)\n", run);
	fputs("\t/* Conditional tests must be explicitly skipped. */\n", run);
	fputs("#if HAVE_SOME_FEATURE\n", run);
	fputs("\tok1(test_some_feature())\n", run);
	fputs("#else\n", run);
	fputs("\tskip(1, \"Don\'t have SOME_FEATURE\")\n", run);
	fputs("#endif\n", run);
	fputs("\n", run);
	fputs("\t/* This exits depending on whether all tests passed */\n", run);
	fputs("\treturn exit_status();\n", run);
	fputs("}\n", run);

	fclose(run);
}	

struct ccanlint has_tests = {
	.key = "has-tests",
	.name = "Module has tests",
	.check = check_has_tests,
	.describe = describe_has_tests,
	.handle = handle_no_tests,
};

REGISTER_TEST(has_tests, NULL);
