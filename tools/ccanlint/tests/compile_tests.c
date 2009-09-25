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
	char *list = talloc_strdup(m, "../tap.o");
	struct ccan_file *i;

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		list = talloc_asprintf_append(list, " %.*s.o",
					      strlen(i->name) - 2, i->name);

	if (link_with_module)
		list = talloc_asprintf_append(list, " ../%s.o", m->basename);

	return list;
}

static char *lib_list(const struct manifest *m)
{
	unsigned int i, num;
	char **libs = get_libs(m, ".", ".", &num);
	char *ret = talloc_strdup(m, "");

	for (i = 0; i < num; i++)
		ret = talloc_asprintf_append(ret, "-l%s ", libs[i]);
	return ret;
}

static int cleanup_testfile(const char *testfile)
{
	unlink(testfile);
	return 0;
}

static char *compile(const void *ctx,
		     struct manifest *m, struct ccan_file *file, bool fail,
		     bool link_with_module)
{
	file->compiled = talloc_strdup(ctx, tempnam("/tmp", "ccanlint"));
	talloc_set_destructor(file->compiled, cleanup_testfile);

	return run_command(m, "cc " CFLAGS " %s -o %s %s %s %s",
			   fail ? "-DFAIL" : "",
			   file->compiled, file->name,
			   obj_list(m, link_with_module), lib_list(m));
}

struct compile_tests_result {
	struct list_node list;
	const char *filename;
	const char *description;
	const char *output;
};

static void *do_compile_tests(struct manifest *m)
{
	struct list_head *list = talloc(m, struct list_head);
	char *cmdout;
	struct ccan_file *i;
	struct compile_tests_result *res;

	list_head_init(list);

	list_for_each(&m->compile_ok_tests, i, list) {
		compile_tests.total_score++;
		cmdout = compile(list, m, i, false, false);
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
		cmdout = compile(m, m, i, false, false);
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
		cmdout = compile(m, m, i, false, true);
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
		cmdout = compile(list, m, i, true, false);
		if (cmdout) {
			res = talloc(list, struct compile_tests_result);
			res->filename = i->name;
			res->description = "failed to compile without -DFAIL";
			res->output = talloc_steal(res, cmdout);
			list_add_tail(list, &res->list);
		} else {
			cmdout = compile(list, m, i, false, false);
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
	.name = "Compile tests succeed",
	.score = score_compile_tests,
	.check = do_compile_tests,
	.describe = describe_compile_tests,
	.can_run = can_build,
};

REGISTER_TEST(compile_tests, &compile_test_helpers, NULL);
