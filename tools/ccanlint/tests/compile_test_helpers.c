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

static char *compile(struct manifest *m,
		     bool keep,
		     struct ccan_file *cfile)
{
	char *output;
	cfile->compiled = maybe_temp_file(m, ".o", keep, cfile->fullname);
	if (compile_object(m, cfile->fullname, ccan_dir, "",
			   cfile->compiled, &output)) {
		talloc_free(output);
		return NULL;
	}
	return output;
}

static void do_compile_test_helpers(struct manifest *m,
				    bool keep,
				    unsigned int *timeleft,
				    struct score *score)
{
	struct ccan_file *i;

	if (list_empty(&m->other_test_c_files))
		score->total = 0;

	list_for_each(&m->other_test_c_files, i, list) {
		char *cmdout = compile(m, keep, i);
		if (cmdout) {
			score->error = "Failed to compile helper C files";
			score_file_error(score, i, 0, cmdout);
		}
	}

	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint compile_test_helpers = {
	.key = "compile-helpers",
	.name = "Module test helper objects compile",
	.check = do_compile_test_helpers,
	.can_run = can_run,
};

REGISTER_TEST(compile_test_helpers, &depends_built, &has_tests, NULL);
