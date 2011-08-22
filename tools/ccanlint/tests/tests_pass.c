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

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static void do_run_tests(struct manifest *m,
			 bool keep,
			 unsigned int *timeleft,
			 struct score *score)
{
	struct list_head *list;
	struct ccan_file *i;
	char *cmdout;

	score->total = 0;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			score->total++;
			if (run_command(m, timeleft, &cmdout, "%s",
					i->compiled))
				score->score++;
			else
				score_file_error(score, i, 0, "%s", cmdout);
		}
	}

	if (score->score == score->total)
		score->pass = true;
}

/* Gcc's warn_unused_result is fascist bullshit. */
#define doesnt_matter()

static void run_under_debugger(struct manifest *m, struct score *score)
{
	char *command;
	struct file_error *first;

	if (!ask("Should I run the first failing test under the debugger?"))
		return;

	first = list_top(&score->per_file_errors, struct file_error, list);
	command = talloc_asprintf(m, "gdb -ex 'break tap.c:132' -ex 'run' %s",
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
	.check = do_run_tests,
	.handle = run_under_debugger,
	.needs = "tests_compile_without_features"
};

REGISTER_TEST(tests_pass_without_features);
