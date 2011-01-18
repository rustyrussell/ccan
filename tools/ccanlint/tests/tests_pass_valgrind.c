#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/foreach/foreach.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/str_talloc/str_talloc.h>
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

REGISTER_TEST(tests_pass_valgrind);

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

/* Example output:
==2749== Conditional jump or move depends on uninitialised value(s)
==2749==    at 0x4026C60: strnlen (mc_replace_strmem.c:263)
==2749==    by 0x40850E3: vfprintf (vfprintf.c:1614)
==2749==    by 0x408EACF: printf (printf.c:35)
==2749==    by 0x8048465: main (in /tmp/foo)
==2749== 
==2749== 1 bytes in 1 blocks are definitely lost in loss record 1 of 1
==2749==    at 0x4025BD3: malloc (vg_replace_malloc.c:236)
==2749==    by 0x8048444: main (in /tmp/foo)
==2749== 
*/

static bool blank_line(const char *line)
{
	return line[strspn(line, "=0123456789 ")] == '\0';
}

static char *get_leaks(const char *output, char **errs)
{
	char *leaks = talloc_strdup(output, "");
	unsigned int i;
	char **lines = strsplit(output, output, "\n");

	*errs = talloc_strdup(output, "");
	for (i = 0; i < talloc_array_length(lines) - 1; i++) {
		if (strstr(lines[i], " lost ")) {
			/* A leak... */
			if (strstr(lines[i], " definitely lost ")) {
				/* Definite leak, report. */
				while (lines[i] && !blank_line(lines[i])) {
					leaks = talloc_append_string(leaks,
								     lines[i]);
					leaks = talloc_append_string(leaks,
								     "\n");
					i++;
				}
			} else
				/* Not definite, ignore. */
				while (lines[i] && !blank_line(lines[i]))
					i++;
		} else {
			/* A real error. */
			while (lines[i] && !blank_line(lines[i])) {
				*errs = talloc_append_string(*errs, lines[i]);
				*errs = talloc_append_string(*errs, "\n");
				i++;
			}
		}
	}
	if (!leaks[0]) {
		talloc_free(leaks);
		leaks = NULL;
	}
	if (!(*errs)[0]) {
		talloc_free(*errs);
		*errs = NULL;
	}
	return leaks;
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

	/* This is slow, so we run once but grab leak info. */
	score->total = 0;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			char *output, *err;
			score->total++;
			/* FIXME: Valgrind's output sucks.  XML is unreadable by
			 * humans, and you can't have both. */
			run_command(score, timeleft, &cmdout,
				    "valgrind -q --error-exitcode=101"
				    " --leak-check=full"
				    " --log-fd=3 %s %s"
				    " 3> valgrind.log",
				    tests_pass_valgrind.options ?
				    tests_pass_valgrind.options : "",
				    i->compiled);
			output = grab_file(i, "valgrind.log", NULL);
			if (!output || output[0] == '\0') {
				err = NULL;
				i->leak_info = NULL;
			} else {
				i->leak_info = get_leaks(output, &err);
			}
			if (err)
				score_file_error(score, i, 0, err);
			else
				score->score++;
		}
	}

	if (score->score == score->total)
		score->pass = true;
}

static void do_leakcheck_vg(struct manifest *m,
			    bool keep,
			    unsigned int *timeleft,
			    struct score *score)
{
	struct ccan_file *i;
	struct list_head *list;
	bool leaks = false;

	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			if (i->leak_info) {
				score_file_error(score, i, 0, i->leak_info);
				leaks = true;
			}
		}
	}

	if (!leaks) {
		score->score = 1;
		score->pass = true;
	}
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
	command = talloc_asprintf(m, "valgrind --db-attach=yes%s %s",
				  tests_pass_valgrind.options ?
				  tests_pass_valgrind.options : "",
				  first->file->compiled);
	if (system(command))
		doesnt_matter();
}

struct ccanlint tests_pass_valgrind = {
	.key = "tests_pass_valgrind",
	.name = "Module's run and api tests succeed under valgrind",
	.can_run = can_run_vg,
	.check = do_run_tests_vg,
	.handle = run_under_debugger_vg,
	.takes_options = true,
	.needs = "tests_pass"
};

struct ccanlint tests_pass_valgrind_noleaks = {
	.key = "tests_pass_valgrind_noleaks",
	.name = "Module's run and api tests have no memory leaks",
	.check = do_leakcheck_vg,
	.needs = "tests_pass_valgrind"
};

REGISTER_TEST(tests_pass_valgrind_noleaks);
