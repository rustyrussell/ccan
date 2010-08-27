#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
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

/* Note: we already test safe_mode in run_tests.c */
static const char *can_run_coverage(struct manifest *m)
{
	unsigned int timeleft = default_timeout_ms;
	char *output = run_command(m, &timeleft, "gcov -h");

	if (output)
		return talloc_asprintf(m, "No gcov support: %s", output);
	return NULL;
}

static char *build_module_objs_with_coverage(struct manifest *m, bool keep,
					     char **modobjs)
{
	struct ccan_file *i;

	*modobjs = talloc_strdup(m, "");
	list_for_each(&m->c_files, i, list) {
		char *err;
		char *fullfile = talloc_asprintf(m, "%s/%s", m->dir, i->name);

		i->cov_compiled = maybe_temp_file(m, "", keep, fullfile);
		err = compile_object(m, fullfile, ccan_dir, "",
				     i->cov_compiled);
		if (err) {
			talloc_free(i->cov_compiled);
			return err;
		}
		*modobjs = talloc_asprintf_append(*modobjs,
						  " %s", i->cov_compiled);
	}
	return NULL;
}

static char *obj_list(const struct manifest *m, const char *modobjs)
{
	char *list;
	struct ccan_file *i;

	/* We expect to be linked with tap, unless that's us. */
	if (!streq(m->basename, "tap"))
		list = talloc_asprintf(m, "%s/ccan/tap.o", ccan_dir);
	else
		list = talloc_strdup(m, "");

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		list = talloc_asprintf_append(list, " %s", i->compiled);

	if (modobjs)
		list = talloc_append_string(list, modobjs);

	/* Other ccan modules (don't need coverage versions of those). */
	list_for_each(&m->dep_dirs, i, list) {
		if (i->compiled)
			list = talloc_asprintf_append(list, " %s", i->compiled);
	}

	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, ".", &num, &m->info_file->compiled);
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
	char *errmsg;

	file->cov_compiled = maybe_temp_file(ctx, "", keep, file->fullname);
	errmsg = compile_and_link(ctx, file->fullname, ccan_dir,
				  obj_list(m, modobjs),
				  COVERAGE_CFLAGS,
				  lib_list(m), file->cov_compiled);
	if (errmsg) {
		talloc_free(file->cov_compiled);
		return errmsg;
	}

	return NULL;
}

struct compile_tests_result {
	struct list_node list;
	const char *filename;
	const char *description;
	const char *output;
};

static void *do_compile_coverage_tests(struct manifest *m,
				       bool keep,
				       unsigned int *timeleft)
{
	struct list_head *list = talloc(m, struct list_head);
	char *cmdout, *modobjs = NULL;
	struct ccan_file *i;
	struct compile_tests_result *res;

	list_head_init(list);

	if (!list_empty(&m->api_tests)) {
		cmdout = build_module_objs_with_coverage(m, keep, &modobjs);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = "Module objects with coverage";
			res->description = "failed to compile";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
			return list;
		}
	}

	list_for_each(&m->run_tests, i, list) {
		compile_tests.total_score++;
		cmdout = cov_compile(m, m, i, NULL, keep);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		}
	}

	list_for_each(&m->api_tests, i, list) {
		compile_tests.total_score++;
		cmdout = cov_compile(m, m, i, modobjs, keep);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		}
	}

	if (list_empty(list)) {
		talloc_free(list);
		list = NULL;
	}

	return list;
}

static const char *describe_compile_coverage_tests(struct manifest *m,
						   void *check_result)
{
	struct list_head *list = check_result;
	struct compile_tests_result *i;
	char *descrip;

	descrip = talloc_strdup(list,
				"Compilation of tests for coverage failed:\n");

	list_for_each(list, i, list)
		descrip = talloc_asprintf_append(descrip, "%s %s\n%s",
						 i->filename, i->description,
						 i->output);
	return descrip;
}

struct ccanlint compile_coverage_tests = {
	.key = "compile-coverage-tests",
	.name = "Module tests compile with profiling",
	.check = do_compile_coverage_tests,
	.describe = describe_compile_coverage_tests,
	.can_run = can_run_coverage,
};

REGISTER_TEST(compile_coverage_tests, &compile_tests, NULL);
