#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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

/* We don't do these under valgrind: too slow! */
static void do_run_tests_no_features(struct manifest *m,
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
			if (verbose >= 2)
				printf("   %s\n", i->name);
			if (run_command(m, timeleft, &cmdout, "%s",
					i->compiled[COMPILE_NOFEAT]))
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

	first = list_top(&score->per_file_errors, struct file_error, list);

	if (!ask("Should I run the first failing test under the debugger?"))
		return;

	command = tal_fmt(m, "gdb -ex 'break tap.c:139' -ex 'run' %s",
			  first->file->compiled[COMPILE_NOFEAT]);
	if (system(command))
		doesnt_matter();
}

struct ccanlint tests_pass_without_features = {
	.key = "tests_pass_without_features",
	.name = "Module's run and api tests pass (without features)",
	.check = do_run_tests_no_features,
	.handle = run_under_debugger,
	.needs = "tests_compile_without_features"
};

REGISTER_TEST(tests_pass_without_features);
