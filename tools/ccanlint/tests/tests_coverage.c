#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/tal/path/path.h>
#include <ccan/tal/grab_file/grab_file.h>
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

static bool find_source_file(const struct manifest *m, const char *filename)
{
	const struct ccan_file *i;

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

/* 1 point for 50%, 2 points for 75%, 3 points for 87.5%...  Bonus for 100%. */
static unsigned int score_coverage(float covered, unsigned total)
{
	float thresh, uncovered = 1.0 - covered;
	unsigned int i;

	if (covered == 1.0)
		return total;

	total--;
	for (i = 0, thresh = 0.5; i < total; i++, thresh /= 2) {
		if (uncovered > thresh)
			break;
	}
	return i;
}


/* FIXME: Don't know how stable this is.  Read cov files directly? */
static void analyze_coverage(struct manifest *m, bool full_gcov,
			     const char *output, struct score *score)
{
	char **lines = tal_strsplit(score, output, "\n", STR_EMPTY_OK);
	float covered_lines = 0.0;
	unsigned int i, total_lines = 0;
	bool lines_matter = false;

	/*
	  Output looks like: (gcov 4.6.3)
	   File '../../../ccan/tdb2/private.h'
	   Lines executed:0.00% of 8
	   /home/ccan/ccan/tdb2/test/run-simple-delete.c:creating 'run-simple-delete.c.gcov'

	   File '../../../ccan/tdb2/tdb.c'
	   Lines executed:0.00% of 450

	 For gcov 4.7.2:

	   File '/home/dwg/src/ccan/ccan/rfc822/test/run-check-check.c'
	   Lines executed:100.00% of 19
	   Creating 'run-check-check.c.gcov'
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
		} else if (full_gcov
			   && (strstr(lines[i], ":creating '")
			       || strstarts(lines[i], "Creating '"))) {
			char *file, *filename, *apostrophe;
			apostrophe = strchr(lines[i], '\'');
			filename = apostrophe + 1;
			apostrophe = strchr(filename, '\'');
			*apostrophe = '\0';
			if (lines_matter) {
				file = grab_file(score, filename);
				if (!file) {
					score->error = tal_fmt(score,
							       "Reading %s",
							       filename);
					return;
				}
				printf("%s", file);
			}
			if (tools_verbose)
				printf("Unlinking %s", filename);
			unlink(filename);
		}
	}

	score->pass = true;

	if (verbose > 1)
		printf("%u of %u lines covered\n",
		       (unsigned)covered_lines, total_lines);

	/* Nothing covered?  We can't tell if there's a source file which
	 * was never executed, or there really is no code to execute, so
	 * assume the latter: this test deserves no score. */
	if (total_lines == 0)
		score->total = score->score = 0;
	else {
		score->total = 6;
		score->score = score_coverage(covered_lines / total_lines,
					      score->total);
	}
}

static void do_run_coverage_tests(struct manifest *m,
				  unsigned int *timeleft, struct score *score)
{
	struct ccan_file *i;
	char *cmdout, *outdir;
	char *covcmd;
	bool full_gcov = (verbose > 1);
	struct list_head *list;
	bool ran_some = false;

	/* This tells gcov where we put those .gcno files. */
	outdir = path_dirname(score,
			      m->info_file->compiled[COMPILE_NORMAL]);
	covcmd = tal_fmt(m, "gcov %s -o %s",
			 full_gcov ? "" : "-n",
			 outdir);

	/* Run them all. */
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			if (run_command(score, timeleft, &cmdout,
					"%s", i->compiled[COMPILE_COVERAGE])) {
				tal_append_fmt(&covcmd, " %s", i->fullname);
			} else {
				score_file_error(score, i, 0,
						 "Running test with coverage"
						 " failed: %s", cmdout);
				return;
			}
			ran_some = true;
		}
	}

	/* No tests at all?  0 out of 0 for you... */
	if (!ran_some) {
		score->total = score->score = 0;
		score->pass = true;
		return;
	}

	/* Now run gcov: we want output even if it succeeds. */
	if (!run_command(score, timeleft, &cmdout, "%s", covcmd)) {
		score->error = tal_fmt(score, "Running gcov: %s", cmdout);
		return;
	}

	analyze_coverage(m, full_gcov, cmdout, score);
}

struct ccanlint tests_coverage = {
	.key = "tests_coverage",
	.name = "Module's tests cover all the code",
	.check = do_run_coverage_tests,
	.needs = "tests_compile_coverage tests_pass"
};

REGISTER_TEST(tests_coverage);
