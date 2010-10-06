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

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static char *obj_list(const struct manifest *m, bool link_with_module)
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

	/* Our own object files. */
	if (link_with_module)
		list_for_each(&m->c_files, i, list)
			list = talloc_asprintf_append(list, " %s", i->compiled);

	/* Other ccan modules. */
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

static char *compile(const void *ctx,
		     struct manifest *m,
		     struct ccan_file *file,
		     bool fail,
		     bool link_with_module,
		     bool keep)
{
	char *errmsg;

	file->compiled = maybe_temp_file(ctx, "", keep, file->fullname);
	errmsg = compile_and_link(ctx, file->fullname, ccan_dir,
				  obj_list(m, link_with_module),
				  fail ? "-DFAIL" : "",
				  lib_list(m), file->compiled);
	if (errmsg) {
		talloc_free(file->compiled);
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

static void *do_compile_tests(struct manifest *m,
			      bool keep,
			      unsigned int *timeleft)
{
	struct list_head *list = talloc(m, struct list_head);
	char *cmdout;
	struct ccan_file *i;
	struct compile_tests_result *res;

	list_head_init(list);

	list_for_each(&m->compile_ok_tests, i, list) {
		compile_tests.total_score++;
		cmdout = compile(list, m, i, false, false, keep);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		}
	}

	list_for_each(&m->run_tests, i, list) {
		compile_tests.total_score++;
		cmdout = compile(m, m, i, false, false, keep);
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
		cmdout = compile(m, m, i, false, true, keep);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		}
	}

	list_for_each(&m->compile_fail_tests, i, list) {
		compile_tests.total_score++;
		cmdout = compile(list, m, i, false, false, false);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile without -DFAIL";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		} else {
			cmdout = compile(list, m, i, true, false, false);
			if (!cmdout) {
				res = talloc(list, struct compile_tests_result);
				res->filename = i->name;
				res->description = "compiled successfully"
					" with -DFAIL";
				res->output = "";
				list_add_tail(list, &res->list);
			}
		}
	}

	if (list_empty(list)) {
		talloc_free(list);
		list = NULL;
	}

	return list;
}

static unsigned int score_compile_tests(struct manifest *m,
					void *check_result)
{
	struct list_head *list = check_result;
	struct compile_tests_result *i;
	unsigned int score = compile_tests.total_score;

	list_for_each(list, i, list)
		score--;
	return score;
}

static const char *describe_compile_tests(struct manifest *m,
					  void *check_result)
{
	struct list_head *list = check_result;
	struct compile_tests_result *i;
	char *descrip = talloc_strdup(list, "Compilation tests failed:\n");

	list_for_each(list, i, list)
		descrip = talloc_asprintf_append(descrip, "%s %s\n%s",
						 i->filename, i->description,
						 i->output);
	return descrip;
}

struct ccanlint compile_tests = {
	.key = "compile-tests",
	.name = "Module tests compile",
	.score = score_compile_tests,
	.check = do_compile_tests,
	.describe = describe_compile_tests,
	.can_run = can_build,
};

REGISTER_TEST(compile_tests, &compile_test_helpers, &build_objs, NULL);
