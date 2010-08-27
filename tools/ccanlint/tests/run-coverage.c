#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str_talloc/str_talloc.h>
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
#include "build-coverage.h"

struct coverage_result {
	float uncovered;
	const char *what;
	const char *output;
};

static bool find_source_file(struct manifest *m, const char *filename)
{
	struct ccan_file *i;

	list_for_each(&m->c_files, i, list) {
		if (streq(i->fullname, filename))
			return true;
	}
	list_for_each(&m->h_files, i, list) {
		if (streq(i->fullname, filename))
			return true;
	}
	return false;
}

/* FIXME: Don't know how stable this is.  Read cov files directly? */
static void analyze_coverage(struct manifest *m,
			     struct coverage_result *res, const char *output)
{
	char **lines = strsplit(res, output, "\n", NULL);
	float covered_lines = 0.0;
	unsigned int i, total_lines = 0;
	bool lines_matter = false;

	/* FIXME: We assume GCOV mentions all files!
	  Output looks like:
	   File '../../../ccan/tdb2/private.h'
	   Lines executed:0.00% of 8

	   File '../../../ccan/tdb2/tdb.c'
	   Lines executed:0.00% of 450
	*/

	for (i = 0; lines[i]; i++) {
		if (strstarts(lines[i], "File '")) {
			char *file = lines[i] + strlen("File '");
			file[strcspn(file, "'")] = '\0';
			if (find_source_file(m, file))
				lines_matter = true;
			else
				lines_matter = false;
		} else if (lines_matter
			   && strstarts(lines[i], "Lines executed:")) {
			float ex;
			unsigned of;
			if (sscanf(lines[i], "Lines executed:%f%% of %u",
				   &ex, &of) != 2)
				errx(1, "Could not parse line '%s'", lines[i]);
			total_lines += of;
			covered_lines += ex / 100.0 * of;
		}
	}

	/* Nothing covered? */
	if (total_lines == 0)
		res->uncovered = 1.0;
	else
		res->uncovered = 1.0 - covered_lines / total_lines;
}

static void *do_run_coverage_tests(struct manifest *m,
				   bool keep,
				   unsigned int *timeleft)
{
	struct coverage_result *res;
	struct ccan_file *i;
	char *cmdout;
	char *olddir;
	char *covcmd;
	bool ok;

	/* We run tests in the module directory, so any paths
	 * referenced can all be module-local. */
	olddir = talloc_getcwd(m);
	if (!olddir)
		err(1, "Could not save cwd");
	if (chdir(m->dir) != 0)
		err(1, "Could not chdir to %s", m->dir);

	res = talloc(m, struct coverage_result);
	res->what = NULL;
	res->uncovered = 1.0;

	/* This tells gcov where we put those .gcno files. */
	covcmd = talloc_asprintf(m, "gcov -n -o %s",
				 talloc_dirname(res, m->info_file->compiled));

	/* Run them all. */
	list_for_each(&m->run_tests, i, list) {
		cmdout = run_command(m, timeleft, i->cov_compiled);
		if (cmdout) {
			res->what = i->fullname;
			res->output = talloc_steal(res, cmdout);
			return res;
		}
		covcmd = talloc_asprintf_append(covcmd, " %s", i->name);
		move_gcov_turd(olddir, i, ".gcda");
	}

	list_for_each(&m->api_tests, i, list) {
		cmdout = run_command(m, timeleft, i->cov_compiled);
		if (cmdout) {
			res->what = i->fullname;
			res->output = talloc_steal(res, cmdout);
			return res;
		}
		covcmd = talloc_asprintf_append(covcmd, " %s", i->name);
		move_gcov_turd(olddir, i, ".gcda");
	}

	/* Now run gcov: we want output even if it succeeds. */
	cmdout = run_with_timeout(m, covcmd, &ok, timeleft);
	if (!ok) {
		res->what = "Running gcov";
		res->output = talloc_steal(res, cmdout);
		return res;
	}

	analyze_coverage(m, res, cmdout);
	return res;
}

/* 1 point for 50%, 2 points for 75%, 3 points for 87.5%... */
static unsigned int score_coverage(struct manifest *m, void *check_result)
{
	struct coverage_result *res = check_result;
	float thresh;
	unsigned int i;

	for (i = 0, thresh = 0.5;
	     i < run_coverage_tests.total_score;
	     i++, thresh /= 2) {
		if (res->uncovered > thresh)
			break;
	}
	return i;
}

static const char *describe_run_coverage_tests(struct manifest *m,
					       void *check_result)
{
	struct coverage_result *res = check_result;

	if (res->what)
		return talloc_asprintf(m, "%s: %s", res->what, res->output);

	return talloc_asprintf(m, "Tests achieved %0.2f%% coverage",
			       (1.0 - res->uncovered) * 100);
}

struct ccanlint run_coverage_tests = {
	.key = "test-coverage",
	.name = "Code coverage of module tests",
	.total_score = 5,
	.score = score_coverage,
	.check = do_run_coverage_tests,
	.describe = describe_run_coverage_tests,
};

REGISTER_TEST(run_coverage_tests, &compile_coverage_tests, NULL);
