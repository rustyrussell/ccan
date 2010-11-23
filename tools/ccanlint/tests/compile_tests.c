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

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

/* FIXME: Merge this into one place. */
static char *obj_list(const struct manifest *m, bool link_with_module)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;
	struct manifest *subm;

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		list = talloc_asprintf_append(list, " %s", i->compiled);

	/* Our own object files. */
	if (link_with_module)
		list_for_each(&m->c_files, i, list)
			list = talloc_asprintf_append(list, " %s", i->compiled);

	/* Other ccan modules. */
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

static bool compile(const void *ctx,
		    struct manifest *m,
		    struct ccan_file *file,
		    bool fail,
		    bool link_with_module,
		    bool keep, char **output)
{
	file->compiled = maybe_temp_file(ctx, "", keep, file->fullname);
	if (!compile_and_link(ctx, file->fullname, ccan_dir,
			      obj_list(m, link_with_module),
			      fail ? "-DFAIL" : "",
			      lib_list(m), file->compiled, output)) {
		talloc_free(file->compiled);
		return false;
	}
	return true;
}

static void do_compile_tests(struct manifest *m,
			     bool keep,
			     unsigned int *timeleft, struct score *score)
{
	char *cmdout;
	struct ccan_file *i;
	struct list_head *list;
	bool errors = false, warnings = false;

	foreach_ptr(list, &m->compile_ok_tests, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			if (!compile(score, m, i, false, list == &m->api_tests,
				     keep, &cmdout)) {
				score->error = "Failed to compile tests";
				score_file_error(score, i, 0, cmdout);
				errors = true;
			} else if (!streq(cmdout, "")) {
				score->error = "Test compiled with warnings";
				score_file_error(score, i, 0, cmdout);
				warnings = true;
			}
		}
	}

	/* The compile fail tests are a bit weird, handle them separately */
	if (errors)
		return;

	/* For historical reasons, "fail" often means "gives warnings" */
	list_for_each(&m->compile_fail_tests, i, list) {
		if (!compile(score, m, i, false, false, false, &cmdout)) {
			score->error = "Failed to compile without -DFAIL";
			score_file_error(score, i, 0, cmdout);
			return;
		}
		if (!streq(cmdout, "")) {
			score->error = "Compile with warnigns without -DFAIL";
			score_file_error(score, i, 0, cmdout);
			return;
		}
		if (compile(score, m, i, true, false, false, &cmdout)
		    && streq(cmdout, "")) {
			score->error = "Compiled successfully with -DFAIL?";
			score_file_error(score, i, 0, NULL);
			return;
		}
	}

	score->pass = true;
	score->total = 2;
	score->score = 1 + !warnings;
}

struct ccanlint compile_tests = {
	.key = "compile-tests",
	.name = "Module tests compile",
	.check = do_compile_tests,
	.can_run = can_build,
};

REGISTER_TEST(compile_tests, &compile_test_helpers, &build_objs, NULL);
