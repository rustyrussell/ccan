#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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
#include "reduce_features.h"
#include "tests_compile.h"

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

char *test_obj_list(const struct manifest *m, bool link_with_module,
		    enum compile_type ctype, enum compile_type own_ctype)
{
	char *list = tal_strdup(m, "");
	struct ccan_file *i;
	struct manifest *subm;

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		tal_append_fmt(&list, " %s", i->compiled[ctype]);

	/* Our own object files. */
	if (link_with_module)
		list_for_each(&m->c_files, i, list)
			tal_append_fmt(&list, " %s", i->compiled[own_ctype]);

	/* Other ccan modules (normal depends). */
	list_for_each(&m->deps, subm, list) {
		if (subm->compiled[ctype])
			tal_append_fmt(&list, " %s", subm->compiled[ctype]);
	}

	/* Other ccan modules (test depends). */
	list_for_each(&m->test_deps, subm, list) {
		if (subm->compiled[ctype])
			tal_append_fmt(&list, " %s", subm->compiled[ctype]);
	}

	return list;
}

char *test_lib_list(const struct manifest *m, enum compile_type ctype)
{
	unsigned int i;
	char **libs;
	char *ret = tal_strdup(m, "");

	libs = get_libs(m, m->dir, "testdepends", get_or_compile_info);
	for (i = 0; libs[i]; i++)
		tal_append_fmt(&ret, "-l%s ", libs[i]);
	return ret;
}

static char *cflags_list(const struct manifest *m, const char *iflags)
{
	unsigned int i;
	char *ret = tal_strdup(m, iflags);

	char **flags = get_cflags(m, m->dir, get_or_compile_info);
	for (i = 0; flags[i]; i++)
		tal_append_fmt(&ret, " %s", flags[i]);
	return ret;
}

static bool compile(const void *ctx,
		    struct manifest *m,
		    struct ccan_file *file,
		    bool fail,
		    bool link_with_module,
		    enum compile_type ctype,
		    char **output)
{
	char *fname, *flags;

	flags = tal_fmt(ctx, "%s%s%s",
			fail ? "-DFAIL " : "",
			cflags,
			ctype == COMPILE_NOFEAT
			? " "REDUCE_FEATURES_FLAGS : "");
	flags = cflags_list(m, flags);

	fname = temp_file(ctx, "", file->fullname);
	if (!compile_and_link(ctx, file->fullname, ccan_dir,
			      test_obj_list(m, link_with_module,
					    ctype, ctype),
			      compiler, flags, test_lib_list(m, ctype), fname,
			      output)) {
		tal_free(fname);
		return false;
	}

	file->compiled[ctype] = fname;
	return true;
}

static void compile_async(const void *ctx,
			  struct manifest *m,
			  struct ccan_file *file,
			  bool link_with_module,
			  enum compile_type ctype,
			  unsigned int time_ms)
{
	char *flags;

	file->compiled[ctype] = temp_file(ctx, "", file->fullname);
	flags = tal_fmt(ctx, "%s%s",
			cflags,
			ctype == COMPILE_NOFEAT
			? " "REDUCE_FEATURES_FLAGS : "");
	flags = cflags_list(m, flags);

	compile_and_link_async(file, time_ms, file->fullname, ccan_dir,
			       test_obj_list(m, link_with_module, ctype, ctype),
			       compiler, flags, test_lib_list(m, ctype),
			       file->compiled[ctype]);
}

static void compile_tests(struct manifest *m,
			  struct score *score,
			  enum compile_type ctype,
			  unsigned int time_ms)
{
	char *cmdout;
	struct ccan_file *i;
	struct list_head *list;
	bool errors = false, warnings = false, ok;

	foreach_ptr(list, &m->compile_ok_tests, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			compile_async(score, m, i,
				      list == &m->api_tests,
				      ctype, time_ms);
		}
	}

	while ((i = collect_command(&ok, &cmdout)) != NULL) {
		if (!ok) {
			score_file_error(score, i, 0,
					 "Compile failed:\n%s",
					 cmdout);
			errors = true;
		} else if (!streq(cmdout, "")) {
			score_file_error(score, i, 0,
					 "Compile gave warnings:\n%s",
					 cmdout);
			warnings = true;
		}
	}

	/* The compile fail tests are a bit weird, handle them separately */
	if (errors)
		return;

	/* For historical reasons, "fail" often means "gives warnings" */
	list_for_each(&m->compile_fail_tests, i, list) {
		if (!compile(score, m, i, false, false, ctype, &cmdout)) {
			score_file_error(score, i, 0,
					 "Compile without -DFAIL failed:\n%s",
					 cmdout);
			return;
		}
		if (!streq(cmdout, "")) {
			score_file_error(score, i, 0,
					 "Compile gave warnings"
					 " without -DFAIL:\n%s",
					 cmdout);
			return;
		}
		if (compile(score, m, i, true, false, ctype, &cmdout)
		    && streq(cmdout, "")) {
			score_file_error(score, i, 0,
					 "Compiled successfully with -DFAIL?");
			return;
		}
		score->total++;
	}

	score->pass = true;
	score->score = score->total - warnings;
}

/* FIXME: If we time out, set *timeleft to 0 */
static void do_compile_tests(struct manifest *m,
			     unsigned int *timeleft, struct score *score)
{
	compile_tests(m, score, COMPILE_NORMAL, *timeleft);
}

struct ccanlint tests_compile = {
	.key = "tests_compile",
	.name = "Module tests compile",
	.check = do_compile_tests,
	.can_run = can_build,
	.needs = "tests_helpers_compile objects_build"
};

REGISTER_TEST(tests_compile);

static const char *features_reduced(struct manifest *m)
{
	if (features_were_reduced)
		return NULL;
	return "No features to turn off";
}

static void do_compile_tests_without_features(struct manifest *m,
					      unsigned int *timeleft,
					      struct score *score)
{
	compile_tests(m, score, COMPILE_NOFEAT, *timeleft);
}

struct ccanlint tests_compile_without_features = {
	.key = "tests_compile_without_features",
	.name = "Module tests compile (without features)",
	.check = do_compile_tests_without_features,
	.can_run = features_reduced,
	.needs = "module_builds tests_helpers_compile_without_features objects_build_without_features"
};
REGISTER_TEST(tests_compile_without_features);
