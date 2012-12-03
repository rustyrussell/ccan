#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
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
#include "reduce_features.h"

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static bool compile(struct manifest *m,
		    struct ccan_file *cfile,
		    const char *flags,
		    enum compile_type ctype,
		    char **output)
{
	cfile->compiled[ctype] = temp_file(m, ".o", cfile->fullname);
	return compile_object(m, cfile->fullname, ccan_dir, compiler, flags,
			      cfile->compiled[ctype], output);
}

static void compile_test_helpers(struct manifest *m,
				 unsigned int *timeleft,
				 struct score *score,
				 const char *flags,
				 enum compile_type ctype)
{
	struct ccan_file *i;
	bool errors = false, warnings = false;

	if (list_empty(&m->other_test_c_files))
		score->total = 0;
	else
		score->total = 2;

	list_for_each(&m->other_test_c_files, i, list) {
		char *cmdout;

		if (!compile(m, i, flags, ctype, &cmdout)) {
			errors = true;
			score_file_error(score, i, 0, "Compile failed:\n%s",
					 cmdout);
		} else if (!streq(cmdout, "")) {
			warnings = true;
			score_file_error(score, i, 0,
					 "Compile gave warnings:\n%s", cmdout);
		}
	}

	if (!errors) {
		score->pass = true;
		score->score = score->total - warnings;
	}
}

static void do_compile_test_helpers(struct manifest *m,
				    unsigned int *timeleft,
				    struct score *score)
{
	compile_test_helpers(m, timeleft, score, cflags, COMPILE_NORMAL);
}

struct ccanlint tests_helpers_compile = {
	.key = "tests_helpers_compile",
	.name = "Module test helper objects compile",
	.check = do_compile_test_helpers,
	.can_run = can_run,
	.needs = "depends_build tests_exist"
};

REGISTER_TEST(tests_helpers_compile);

static const char *features_reduced(struct manifest *m)
{
	if (features_were_reduced)
		return NULL;
	return "No features to turn off";
}

static void do_compile_test_helpers_without_features(struct manifest *m,
						     unsigned int *timeleft,
						     struct score *score)
{
	char *flags;

	flags = tal_fmt(score, "%s %s", cflags, REDUCE_FEATURES_FLAGS);

	compile_test_helpers(m, timeleft, score, flags, COMPILE_NOFEAT);
}

struct ccanlint tests_helpers_compile_without_features = {
	.key = "tests_helpers_compile_without_features",
	.name = "Module tests helpers compile (without features)",
	.check = do_compile_test_helpers_without_features,
	.can_run = features_reduced,
	.needs = "depends_build_without_features tests_exist"
};
REGISTER_TEST(tests_helpers_compile_without_features);
