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

static bool compile_obj(struct ccan_file *c_file, char *objfile, char **report)
{
	char *err;

	err = compile_object(objfile, objfile, c_file->name);
	if (err) {
		if (*report)
			*report = talloc_append_string(*report, err);
		else
			*report = err;
		return false;
	}
	return true;
}

static int cleanup_obj(char *objfile)
{
	unlink(objfile);
	return 0;
}

static void *check_objs_build(struct manifest *m)
{
	char *report = NULL;
	struct ccan_file *i;

	/* One point for each obj file. */
	list_for_each(&m->c_files, i, list)
		build_objs.total_score++;

	list_for_each(&m->c_files, i, list) {
		char *objfile = talloc_strdup(m, i->name);
		objfile[strlen(objfile)-1] = 'o';

		if (compile_obj(i, objfile, &report))
			talloc_set_destructor(objfile, cleanup_obj);
	}
	return report;
}

static const char *describe_objs_build(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "%s", (char *)check_result);
}

struct ccanlint build_objs = {
	.name = "Module object files can be built",
	.check = check_objs_build,
	.describe = describe_objs_build,
	.can_run = can_build,
};

REGISTER_TEST(build_objs, &depends_exist, NULL);
