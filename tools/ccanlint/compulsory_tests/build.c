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

static char *obj_list(const struct manifest *m)
{
	char *list = talloc_strdup(m, "");
	struct ccan_file *i;

	/* Objects from all the C files. */
	list_for_each(&m->c_files, i, list)
		list = talloc_asprintf_append(list, "%s ", i->compiled);

	return list;
}

static void *do_build(struct manifest *m)
{
	char *filename, *err;

	if (list_empty(&m->c_files)) {
		/* No files?  No score, but we "pass". */
		build.total_score = 0;
		return NULL;
	}
	filename = link_objects(m, obj_list(m), &err);
	if (filename) {
		char *realname = talloc_asprintf(m, "%s.o", m->dir);
		/* We leave this object file around, all built. */
		if (!move_file(filename, realname))
			return talloc_asprintf(m, "Failed to rename %s to %s",
					       filename, realname);
		return NULL;
	}
	return err;
}

static const char *describe_build(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "The object file for the module didn't build:\n"
			       "%s", (char *)check_result);
}

struct ccanlint build = {
	.key = "build",
	.name = "Module can be built",
	.total_score = 1,
	.check = do_build,
	.describe = describe_build,
	.can_run = can_build,
};

REGISTER_TEST(build, &depends_built, NULL);
