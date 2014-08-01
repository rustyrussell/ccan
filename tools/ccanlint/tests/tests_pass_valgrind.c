#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/take/take.h>
#include <ccan/foreach/foreach.h>
#include <ccan/tal/grab_file/grab_file.h>
#include "tests_pass.h"
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
	if (!do_valgrind)
		return tal_fmt(m, "No valgrind support");
	return NULL;
}

static void do_leakcheck_vg(struct manifest *m,
			    unsigned int *timeleft,
			    struct score *score);

static struct ccanlint tests_pass_valgrind_noleaks = {
	.key = "tests_pass_valgrind_noleaks",
	.name = "Module's run and api tests have no memory leaks",
	.check = do_leakcheck_vg,
	.takes_options = true,
	.needs = "tests_pass_valgrind"
};
REGISTER_TEST(tests_pass_valgrind_noleaks);


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

/* Removes matching lines from lines array, returns them.  FIXME: Hacky. */
static char **extract_matching(const char *prefix, char *lines[])
{
	unsigned int i, num_ret = 0;
	char **ret = tal_arr(lines, char *, tal_count(lines));

	for (i = 0; i < tal_count(lines) - 1; i++) {
		if (strstarts(lines[i], prefix)) {
			ret[num_ret++] = tal_strdup(ret, lines[i]);
			lines[i] = (char *)"";
		}
	}
	ret[num_ret++] = NULL;

	/* Make sure tal_count is correct! */
	tal_resize(&ret, num_ret);
	return ret;
}

static char *get_leaks(char *lines[], char **errs)
{
	char *leaks = tal_strdup(lines, "");
	unsigned int i;

	for (i = 0; i < tal_count(lines) - 1; i++) {
		if (strstr(lines[i], " lost ")) {
			/* A leak... */
			if (strstr(lines[i], " definitely lost ")) {
				/* Definite leak, report. */
				while (lines[i] && !blank_line(lines[i])) {
					tal_append_fmt(&leaks, "%s\n",
						       lines[i]);
					i++;
				}
			} else
				/* Not definite, ignore. */
				while (lines[i] && !blank_line(lines[i]))
					i++;
		} else {
			/* A real error. */
			while (lines[i] && !blank_line(lines[i])) {
				if (!*errs)
					*errs = tal_fmt(NULL, "%s\n", lines[i]);
				else
					tal_append_fmt(errs, "%s\n", lines[i]);
				i++;
			}
		}
	}
	return leaks;
}

/* Returns leaks, and sets errs[] */
static char *analyze_output(const char *output, char **errs)
{
	char *leaks = tal_strdup(output, "");
	unsigned int i;
	char **lines = tal_strsplit(output, output, "\n", STR_EMPTY_OK);

	*errs = tal_strdup(output, "");
	for (i = 0; i < tal_count(lines) - 1; i++) {
		unsigned int preflen = strspn(lines[i], "=0123456789");
		char *prefix, **sublines;

		/* Ignore erased lines, or weird stuff. */
		if (preflen == 0)
			continue;

		prefix = tal_strndup(output, lines[i], preflen);
		sublines = extract_matching(prefix, lines);

		leaks = tal_strcat(output, take(leaks),
				   take(get_leaks(sublines, errs)));
	}

	if (!leaks[0]) {
		tal_free(leaks);
		leaks = NULL;
	}
	if (!(*errs)[0]) {
		tal_free(*errs);
		*errs = NULL;
	}
	return leaks;
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

/* FIXME: Run examples, too! */
static void do_run_tests_vg(struct manifest *m,
			    unsigned int *timeleft,
			    struct score *score)
{
	struct ccan_file *i;
	struct list_head *list;

	/* This is slow, so we run once but grab leak info. */
	score->total = 0;
	score->pass = true;
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			char *err, *output;
			const char *options;

			score->total++;
			options = concat(score,
					 per_file_options(&tests_pass_valgrind,
							  i));
			if (streq(options, "FAIL")) {
				i->leak_info = NULL;
				continue;
			}

			output = grab_file(i, i->valgrind_log);
			/* No valgrind errors? */
			if (!output || output[0] == '\0') {
				err = NULL;
				i->leak_info = NULL;
			} else {
				i->leak_info = analyze_output(output, &err);
			}
			if (err) {
				score_file_error(score, i, 0, "%s", err);
				score->pass = false;
			} else
				score->score++;
		}
	}
}

static void do_leakcheck_vg(struct manifest *m,
			    unsigned int *timeleft,
			    struct score *score)
{
	struct ccan_file *i;
	struct list_head *list;
	char **options;
	bool leaks = false;

	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			options = per_file_options(&tests_pass_valgrind_noleaks,
						   i);
			if (options[0]) {
				if (streq(options[0], "FAIL")) {
					leaks = true;
					continue;
				}
				errx(1, "Unknown leakcheck options '%s'",
				     options[0]);
			}

			if (i->leak_info) {
				score_file_error(score, i, 0, "%s",
						 i->leak_info);
				leaks = true;
			}
		}
	}

	/* FIXME: We don't fail for this, since many tests leak. */ 
	score->pass = true;
	if (!leaks) {
		score->score = 1;
	}
}

/* Gcc's warn_unused_result is fascist bullshit. */
#define doesnt_matter()

static void run_under_debugger_vg(struct manifest *m, struct score *score)
{
	struct file_error *first;
	char *command;

	/* Don't ask anything if they suppressed tests. */
	if (score->pass)
		return;

	if (!ask("Should I run the first failing test under the debugger?"))
		return;

	first = list_top(&score->per_file_errors, struct file_error, list);
	command = tal_fmt(m, "valgrind --leak-check=full --db-attach=yes%s %s %s",
			  concat(score, per_file_options(&tests_pass_valgrind,
							 first->file)),
			  valgrind_suppress, first->file->compiled[COMPILE_NORMAL]);
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
REGISTER_TEST(tests_pass_valgrind);
