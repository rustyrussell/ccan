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

/* Note: we already test safe_mode in run_tests.c */
static const char *can_run_vg(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output;

	if (!run_command(m, &timeleft, &output,
			 "valgrind -q --error-exitcode=0 true"))
		return talloc_asprintf(m, "No valgrind support: %s", output);
	return NULL;
}

/* FIXME: Run examples, too! */
static void do_run_tests_vg(struct manifest *m,
			     bool keep,
			    unsigned int *timeleft,
			    struct score *score)
{
	struct ccan_file *i;
	struct list_head *list;
	char *cmdout;

	score->total = 0;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			score->total++;
			if (run_command(score, timeleft, &cmdout,
					"valgrind -q --error-exitcode=100 %s",
					i->compiled)) {
				score->score++;
			} else {
				score->error = "Running under valgrind";
				score_file_error(score, i, 0, cmdout);
			}
		}
	}

	if (score->score == score->total)
		score->pass = true;
}

/* Gcc's warn_unused_result is fascist bullshit. */
#define doesnt_matter()

static void run_under_debugger_vg(struct manifest *m, struct score *score)
{
	struct file_error *first;
	char *command;

	if (!ask("Should I run the first failing test under the debugger?"))
		return;

	first = list_top(&score->per_file_errors, struct file_error, list);
	command = talloc_asprintf(m, "valgrind --db-attach=yes %s",
				  first->file->compiled);
	if (system(command))
		doesnt_matter();
}

struct ccanlint run_tests_vg = {
	.key = "valgrind-tests",
	.name = "Module's run and api tests succeed under valgrind",
	.can_run = can_run_vg,
	.check = do_run_tests_vg,
	.handle = run_under_debugger_vg
};

REGISTER_TEST(run_tests_vg, &run_tests, NULL);
