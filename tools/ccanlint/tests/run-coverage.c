#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/grab_file/grab_file.h>
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

struct coverage_result {
	float uncovered;
	const char *what;
	char *output;
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
			     struct coverage_result *res, const char *output,
			     bool full_gcov)
{
	char **lines = strsplit(res, output, "\n", NULL);
	float covered_lines = 0.0;
	unsigned int i, total_lines = 0;
	bool lines_matter = false;

	/*
	  Output looks like:
	   File '../../../ccan/tdb2/private.h'
	   Lines executed:0.00% of 8
	   /home/ccan/ccan/tdb2/test/run-simple-delete.c:creating 'run-simple-delete.c.gcov'

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
		} else if (full_gcov && strstr(lines[i], ":creating '")) {
			char *file, *filename, *apostrophe;
			apostrophe = strchr(lines[i], '\'');
			filename = apostrophe + 1;
			apostrophe = strchr(filename, '\'');
			*apostrophe = '\0';
			if (lines_matter) {
				file = grab_file(res, filename, NULL);
				if (!file) {
					res->what = talloc_asprintf(res,
							    "Reading %s",
							    filename);
					res->output = talloc_strdup(res,
							    strerror(errno));
					return;
				}
				res->output = talloc_append_string(res->output,
								   file);
			}
			if (tools_verbose)
				printf("Unlinking %s", filename);
			unlink(filename);
		}
	}

	/* Nothing covered?  We can't tell if there's a source file which
	 * was never executed, or there really is no code to execute, so
	 * assume the latter: this test deserves no score. */
	if (total_lines == 0) {
		res->uncovered = 1.0;
		run_coverage_tests.total_score = 0;
	} else
		res->uncovered = 1.0 - covered_lines / total_lines;
}

static void *do_run_coverage_tests(struct manifest *m,
				   bool keep,
				   unsigned int *timeleft)
{
	struct coverage_result *res;
	struct ccan_file *i;
	char *cmdout;
	char *covcmd;
	bool ok;
	bool full_gcov = (verbose > 1);

	res = talloc(m, struct coverage_result);
	res->what = NULL;
	res->output = talloc_strdup(res, "");
	res->uncovered = 1.0;

	/* This tells gcov where we put those .gcno files. */
	covcmd = talloc_asprintf(m, "gcov %s -o %s",
				 full_gcov ? "" : "-n",
				 talloc_dirname(res, m->info_file->compiled));

	/* Run them all. */
	list_for_each(&m->run_tests, i, list) {
		cmdout = run_command(m, timeleft, i->cov_compiled);
		if (cmdout) {
			res->what = i->fullname;
			res->output = talloc_steal(res, cmdout);
			return res;
		}
		covcmd = talloc_asprintf_append(covcmd, " %s", i->fullname);
	}

	list_for_each(&m->api_tests, i, list) {
		cmdout = run_command(m, timeleft, i->cov_compiled);
		if (cmdout) {
			res->what = i->fullname;
			res->output = talloc_steal(res, cmdout);
			return res;
		}
		covcmd = talloc_asprintf_append(covcmd, " %s", i->fullname);
	}

	/* Now run gcov: we want output even if it succeeds. */
	cmdout = run_with_timeout(m, covcmd, &ok, timeleft);
	if (!ok) {
		res->what = "Running gcov";
		res->output = talloc_steal(res, cmdout);
		return res;
	}

	analyze_coverage(m, res, cmdout, full_gcov);

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
	bool full_gcov = (verbose > 1);
	char *ret;

	if (res->what)
		return talloc_asprintf(m, "%s: %s", res->what, res->output);

	if (!verbose)
		return NULL;

	ret = talloc_asprintf(m, "Tests achieved %0.2f%% coverage",
			      (1.0 - res->uncovered) * 100);
	if (full_gcov)
		ret = talloc_asprintf_append(ret, "\n%s", res->output);
	return ret;
}

struct ccanlint run_coverage_tests = {
	.key = "test-coverage",
	.name = "Code coverage of module tests",
	.total_score = 5,
	.score = score_coverage,
	.check = do_run_coverage_tests,
	.describe = describe_run_coverage_tests,
};

REGISTER_TEST(run_coverage_tests, &compile_coverage_tests, &run_tests, NULL);
