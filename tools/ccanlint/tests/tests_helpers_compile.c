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

static const char *can_run(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static bool compile(struct manifest *m,
		    bool keep,
		    struct ccan_file *cfile,
		    char **output)
{
	cfile->compiled = maybe_temp_file(m, ".o", keep, cfile->fullname);
	return compile_object(m, cfile->fullname, ccan_dir, "",
			      cfile->compiled, output);
}

static void do_compile_test_helpers(struct manifest *m,
				    bool keep,
				    unsigned int *timeleft,
				    struct score *score)
{
	struct ccan_file *i;
	bool errors = false, warnings = false;

	if (list_empty(&m->other_test_c_files))
		score->total = 0;
	else
		score->total = 2;

	list_for_each(&m->other_test_c_files, i, list) {
		char *cmdout;

		if (!compile(m, keep, i, &cmdout)) {
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

struct ccanlint tests_helpers_compile = {
	.key = "tests_helpers_compile",
	.name = "Module test helper objects compile",
	.check = do_compile_test_helpers,
	.can_run = can_run,
	.needs = "depends_build tests_exist"
};

REGISTER_TEST(tests_helpers_compile);
