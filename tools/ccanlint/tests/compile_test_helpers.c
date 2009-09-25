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

static int cleanup_testfile(char *testfile)
{
	unlink(testfile);
	return 0;
}

static char *objname(const void *ctx, const char *cfile)
{
	return talloc_asprintf(ctx, "%.*s.o ", strlen(cfile) - 2, cfile);
}

static char *compile(struct manifest *m, const char *cfile)
{
	char *obj;

	obj = objname(m, cfile);
	talloc_set_destructor(obj, cleanup_testfile);
	return run_command(m, "cc " CFLAGS " -c -o %s %s", obj, cfile);
}

static void *do_compile_test_helpers(struct manifest *m)
{
	char *cmdout = NULL;
	struct ccan_file *i;

	list_for_each(&m->other_test_c_files, i, list) {
		compile_tests.total_score++;
		cmdout = compile(m, i->name);
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
	.name = "Compiling test helper files",
	.total_score = 1,
	.check = do_compile_test_helpers,
	.describe = describe_compile_test_helpers,
	.can_run = can_build,
};

REGISTER_TEST(compile_test_helpers, &depends_built);
