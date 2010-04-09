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

static char *compile(struct manifest *m, struct ccan_file *cfile)
{
	char *err;
	char *fullfile = talloc_asprintf(m, "%s/%s", m->dir, cfile->name);

	cfile->compiled = compile_object(m, fullfile, ccan_dir, &err);
	if (cfile->compiled)
		return NULL;
	return err;
}

static void *do_compile_test_helpers(struct manifest *m)
{
	char *cmdout = NULL;
	struct ccan_file *i;

	list_for_each(&m->other_test_c_files, i, list) {
		compile_tests.total_score++;
		cmdout = compile(m, i);
		if (cmdout)
			return talloc_asprintf(m,
					       "Failed to compile helper C"
					       " code file %s:\n%s",
					       i->name, cmdout);
	}
	return NULL;
}

static const char *describe_compile_test_helpers(struct manifest *m,
						 void *check_result)
{
	return check_result;
}

struct ccanlint compile_test_helpers = {
	.key = "compile-helpers",
	.name = "Module test helper objects compile",
	.total_score = 1,
	.check = do_compile_test_helpers,
	.describe = describe_compile_test_helpers,
	.can_run = can_build,
};

REGISTER_TEST(compile_test_helpers, &depends_built);
