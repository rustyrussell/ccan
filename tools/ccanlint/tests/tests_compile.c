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
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;
	struct manifest *subm;

	/* Objects from any other C files. */
	list_for_each(&m->other_test_c_files, i, list)
		list = talloc_asprintf_append(list, " %s",
					      i->compiled[ctype]);

	/* Our own object files. */
	if (link_with_module)
		list_for_each(&m->c_files, i, list)
			list = talloc_asprintf_append(list, " %s",
						      i->compiled[own_ctype]);

	/* Other ccan modules. */
	list_for_each(&m->deps, subm, list) {
		if (subm->compiled[ctype])
			list = talloc_asprintf_append(list, " %s",
						      subm->compiled[ctype]);
	}

	return list;
}

char *lib_list(const struct manifest *m, enum compile_type ctype)
{
	unsigned int i, num;
	char **libs = get_libs(m, m->dir, &num,
			       &m->info_file->compiled[ctype]);
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
		    bool keep,
		    enum compile_type ctype,
		    char **output)
{
	char *fname, *flags;

	flags = talloc_asprintf(ctx, "%s%s%s",
				fail ? "-DFAIL " : "",
				cflags,
				ctype == COMPILE_NOFEAT
				? " "REDUCE_FEATURES_FLAGS : "");

	fname = maybe_temp_file(ctx, "", keep, file->fullname);
	if (!compile_and_link(ctx, file->fullname, ccan_dir,
			      test_obj_list(m, link_with_module,
					    ctype, ctype),
			      compiler, flags, lib_list(m, ctype), fname,
			      output)) {
		talloc_free(fname);
		return false;
	}

	file->compiled[ctype] = fname;
	return true;
}

static void compile_tests(struct manifest *m, bool keep,
			  struct score *score,
			  enum compile_type ctype)
{
	char *cmdout;
	struct ccan_file *i;
	struct list_head *list;
	bool errors = false, warnings = false;

	foreach_ptr(list, &m->compile_ok_tests, &m->run_tests, &m->api_tests) {
		list_for_each(list, i, list) {
			if (!compile(score, m, i, false,
				     list == &m->api_tests, keep,
				     ctype, &cmdout)) {
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
	}

	/* The compile fail tests are a bit weird, handle them separately */
	if (errors)
		return;

	/* For historical reasons, "fail" often means "gives warnings" */
	list_for_each(&m->compile_fail_tests, i, list) {
		if (!compile(score, m, i, false, false, false,
			     ctype, &cmdout)) {
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
		if (compile(score, m, i, true, false, false,
			    ctype, &cmdout)
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

static void do_compile_tests(struct manifest *m,
			     bool keep,
			     unsigned int *timeleft, struct score *score)
{
	compile_tests(m, keep, score, COMPILE_NORMAL);
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
					      bool keep,
					      unsigned int *timeleft,
					      struct score *score)
{
	compile_tests(m, keep, score, COMPILE_NOFEAT);
}

struct ccanlint tests_compile_without_features = {
	.key = "tests_compile_without_features",
	.name = "Module tests compile (without features)",
	.check = do_compile_tests_without_features,
	.can_run = features_reduced,
	.needs = "tests_helpers_compile_without_features objects_build_without_features"
};
REGISTER_TEST(tests_compile_without_features);
