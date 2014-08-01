#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/take/take.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
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
#include <string.h>
#include <ctype.h>
#include "tests_pass.h"

bool do_valgrind = false;
const char *valgrind_suppress = "";

static const char *can_run(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output;
	if (safe_mode)
		return "Safe mode enabled";

	if (!is_excluded("tests_pass_valgrind")
	    && run_command(m, &timeleft, &output,
			   "valgrind -q true")) {
		const char *sfile;

		do_valgrind = true;

		/* Check for suppressions file for all of CCAN. */
		sfile = path_join(m, ccan_dir, ".valgrind_suppressions");
		if (path_is_file(sfile))
			valgrind_suppress = tal_fmt(m, "--suppressions=%s",
						    sfile);
	}

	return NULL;
}

static const char *concat(struct score *score, char *bits[])
{
	unsigned int i;
	char *ret = tal_strdup(score, "");

	for (i = 0; bits[i]; i++) {
		if (i)
			ret = tal_strcat(score, take(ret), " ");
		ret = tal_strcat(score, take(ret), bits[i]);
	}
	return ret;
}

static void run_test(void *ctx,
		     struct manifest *m,
		     unsigned int *timeleft,
		     struct ccan_file *i)
{
	if (do_valgrind) {
		const char *options;
		options = concat(ctx,
				 per_file_options(&tests_pass_valgrind, i));

		if (!streq(options, "FAIL")) {
			/* FIXME: Valgrind's output sucks.  XML is
			 * unreadable by humans *and* doesn't support
			 * children reporting. */
			i->valgrind_log = tal_fmt(m,
					  "%s.valgrind-log",
					  i->compiled[COMPILE_NORMAL]);

			run_command_async(i, *timeleft,
					  "valgrind -q"
					  " --leak-check=full"
					  " --log-fd=3 %s %s %s"
					  " 3> %s",
					  valgrind_suppress, options,
					  i->compiled[COMPILE_NORMAL],
					  i->valgrind_log);
			return;
		}
	}

	run_command_async(i, *timeleft, "%s",
			  i->compiled[COMPILE_NORMAL]);
}

static void do_run_tests(struct manifest *m,
			 unsigned int *timeleft,
			 struct score *score)
{
	struct list_head *list;
	struct ccan_file *i;
	char *cmdout;
	bool ok;

	score->total = 0;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			score->total++;
			if (verbose >= 2)
				printf("   %s...\n", i->name);
			run_test(score, m, timeleft, i);
		}
	}

	while ((i = collect_command(&ok, &cmdout)) != NULL) {
		if (!ok)
			score_file_error(score, i, 0, "%s", cmdout);
		else
			score->score++;
		if (verbose >= 2)
			printf("   ...%s\n", i->name);
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
			  first->file->compiled[COMPILE_NORMAL]);
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
