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
static const char *can_run_coverage(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output;

	if (!run_command(m, &timeleft, &output, "gcov -h"))
		return talloc_asprintf(m, "No gcov support: %s", output);
	return NULL;
}

static bool build_module_objs_with_coverage(struct manifest *m, bool keep,
					    struct score *score,
					    char **modobjs)
{
	struct ccan_file *i;

	*modobjs = talloc_strdup(m, "");
	list_for_each(&m->c_files, i, list) {
		char *err;
		char *fullfile = talloc_asprintf(m, "%s/%s", m->dir, i->name);

		i->cov_compiled = maybe_temp_file(m, "", keep, fullfile);
		if (!compile_object(m, fullfile, ccan_dir, compiler, cflags,
				    i->cov_compiled, &err)) {
			score_file_error(score, i, 0, "%s", err);
			talloc_free(i->cov_compiled);
			i->cov_compiled = NULL;
			return false;
		}
		*modobjs = talloc_asprintf_append(*modobjs,
						  " %s", i->cov_compiled);
	}
	return true;
}

/* FIXME: Merge this into one place. */
static char *obj_list(const struct manifest *m, const char *modobjs)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;
	struct manifest *subm;

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		list = talloc_asprintf_append(list, " %s", i->compiled);

	if (modobjs)
		list = talloc_append_string(list, modobjs);

	/* Other ccan modules (don't need coverage versions of those). */
	list_for_each(&m->deps, subm, list) {
		if (subm->compiled)
			list = talloc_asprintf_append(list, " %s",
						      subm->compiled);
	}

	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, m->dir, &num, &m->info_file->compiled);
	char *ret = talloc_strdup(m, "");

	for (i = 0; i < num; i++)
		ret = talloc_asprintf_append(ret, "-l%s ", libs[i]);
	return ret;
}

static char *cov_compile(const void *ctx,
			 struct manifest *m,
			 struct ccan_file *file,
			 const char *modobjs,
			 bool keep)
{
	char *output;
	char *f = talloc_asprintf(ctx, "%s %s", cflags, COVERAGE_CFLAGS);

	file->cov_compiled = maybe_temp_file(ctx, "", keep, file->fullname);
	if (!compile_and_link(ctx, file->fullname, ccan_dir,
			      obj_list(m, modobjs),
			      compiler, f,
			      lib_list(m), file->cov_compiled, &output)) {
		talloc_free(file->cov_compiled);
		file->cov_compiled = NULL;
		return output;
	}
	talloc_free(output);
	return NULL;
}

/* FIXME: Coverage from testable examples as well. */
static void do_compile_coverage_tests(struct manifest *m,
				      bool keep,
				      unsigned int *timeleft,
				      struct score *score)
{
	char *cmdout, *modobjs = NULL;
	struct ccan_file *i;
	struct list_head *h;

	if (!list_empty(&m->api_tests)
	    && !build_module_objs_with_coverage(m, keep, score, &modobjs)) {
		score->error = talloc_strdup(score,
			     "Failed to compile module objects with coverage");
		return;
	}

	foreach_ptr(h, &m->run_tests, &m->api_tests) {
		list_for_each(h, i, list) {
			cmdout = cov_compile(m, m, i,
					     h == &m->api_tests
					     ? modobjs : NULL,
					     keep);
			if (cmdout) {
				score_file_error(score, i, 0,
				  "Failed to compile test with coverage: %s",
				  cmdout);
			}
		}
	}
	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint tests_compile_coverage = {
	.key = "tests_compile_coverage",
	.name = "Module tests compile with " COVERAGE_CFLAGS,
	.check = do_compile_coverage_tests,
	.can_run = can_run_coverage,
	.needs = "tests_compile"
};

REGISTER_TEST(tests_compile_coverage);
