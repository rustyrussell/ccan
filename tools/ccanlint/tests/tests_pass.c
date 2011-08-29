#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
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
#include "tests_pass.h"

bool do_valgrind = false;

static const char *can_run(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output;
	if (safe_mode)
		return "Safe mode enabled";

	if (!is_excluded("tests_pass_valgrind")
	    && run_command(m, &timeleft, &output,
			   "valgrind -q true"))
		do_valgrind = true;

	return NULL;
}

static const char *concat(struct score *score, char *bits[])
{
	unsigned int i;
	char *ret = talloc_strdup(score, "");

	for (i = 0; bits[i]; i++) {
		if (i)
			ret = talloc_append_string(ret, " ");
		ret = talloc_append_string(ret, bits[i]);
	}
	return ret;
}

static bool run_test(void *ctx,
		     struct manifest *m,
		     unsigned int *timeleft, char **cmdout,
		     struct ccan_file *i,
		     bool use_valgrind)
{
	if (use_valgrind) {
		const char *options;
		options = concat(ctx,
				 per_file_options(&tests_pass_valgrind, i));

		if (!streq(options, "FAIL")) {
			/* FIXME: Valgrind's output sucks.  XML is
			 * unreadable by humans *and* doesn't support
			 * children reporting. */
			i->valgrind_log = talloc_asprintf(m,
							  "%s.valgrind-log",
							  i->compiled);
			talloc_set_destructor(i->valgrind_log,
					      unlink_file_destructor);

			return run_command(ctx, timeleft, cmdout,
					   "valgrind -q"
					   " --leak-check=full"
					   " --log-fd=3 %s %s"
					   " 3> %s",
					   options,
					   i->compiled, i->valgrind_log);
		}
	}

	return run_command(m, timeleft, cmdout, "%s", i->compiled);
}

static void run_tests(struct manifest *m,
		      bool keep,
		      unsigned int *timeleft,
		      struct score *score,
		      bool use_valgrind)
{
	struct list_head *list;
	struct ccan_file *i;
	char *cmdout;

	score->total = 0;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			score->total++;
			if (run_test(score, m, timeleft, &cmdout, i,
				     use_valgrind))
				score->score++;
			else
				score_file_error(score, i, 0, "%s", cmdout);
		}
	}

	if (score->score == score->total)
		score->pass = true;
}

static void do_run_tests(struct manifest *m,
			 bool keep,
			 unsigned int *timeleft,
			 struct score *score)
{
	run_tests(m, keep, timeleft, score, do_valgrind);
}

static void do_run_tests_without_features(struct manifest *m,
					  bool keep,
					  unsigned int *timeleft,
					  struct score *score)
{
	run_tests(m, keep, timeleft, score, false);
}

/* Gcc's warn_unused_result is fascist bullshit. */
#define doesnt_matter()

static void run_under_debugger(struct manifest *m, struct score *score)
{
	char *command;
	struct file_error *first;

	first = list_top(&score->per_file_errors, struct file_error, list);

	if (!ask("Should I run the first failing test under the debugger?"))
		return;

	command = talloc_asprintf(m, "gdb -ex 'break tap.c:139' -ex 'run' %s",
				  first->file->compiled);
	if (system(command))
		doesnt_matter();
}

struct ccanlint tests_pass = {
	.key = "tests_pass",
	.name = "Module's run and api tests pass",
	.check = do_run_tests,
	.handle = run_under_debugger,
	.can_run = can_run,
	.needs = "tests_compile"
};

REGISTER_TEST(tests_pass);

struct ccanlint tests_pass_without_features = {
	.key = "tests_pass_without_features",
	.name = "Module's run and api tests pass (without features)",
	.check = do_run_tests_without_features,
	.handle = run_under_debugger,
	.needs = "tests_compile_without_features"
};

REGISTER_TEST(tests_pass_without_features);
