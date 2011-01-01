#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/grab_file/grab_file.h>
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
	char **lines = strsplit(score, output, "\n", NULL);
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
				file = grab_file(score, filename, NULL);
				if (!file) {
					score->error = talloc_asprintf(score,
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
				  bool keep,
				  unsigned int *timeleft, struct score *score)
{
	struct ccan_file *i;
	char *cmdout;
	char *covcmd;
	bool full_gcov = (verbose > 1);
	struct list_head *list;

	/* This tells gcov where we put those .gcno files. */
	covcmd = talloc_asprintf(m, "gcov %s -o %s",
				 full_gcov ? "" : "-n",
				 talloc_dirname(score, m->info_file->compiled));

	/* Run them all. */
	foreach_ptr(list, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			if (run_command(score, timeleft, &cmdout,
					"%s", i->cov_compiled)) {
				covcmd = talloc_asprintf_append(covcmd, " %s",
								i->fullname);
			} else {
				score->error = "Running test with coverage";
				score_file_error(score, i, 0, cmdout);
				return;
			}
		}
	}

	/* Now run gcov: we want output even if it succeeds. */
	if (!run_command(score, timeleft, &cmdout, "%s", covcmd)) {
		score->error = talloc_asprintf(score, "Running gcov: %s",
					       cmdout);
		return;
	}

	analyze_coverage(m, full_gcov, cmdout, score);
}

struct ccanlint run_coverage_tests = {
	.key = "test-coverage",
	.name = "Code coverage of module tests",
	.check = do_run_coverage_tests,
};

REGISTER_TEST(run_coverage_tests, &compile_coverage_tests, &run_tests, NULL);
