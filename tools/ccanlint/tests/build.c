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

	/* Object from all the C files. */
	list_for_each(&m->c_files, i, list)
		list = talloc_asprintf_append(list, "%.*s.o ",
					      strlen(i->name) - 2, i->name);

	return list;
}

/* We leave this object file around after ccanlint runs, all built. */
static void *do_build(struct manifest *m)
{
	if (list_empty(&m->c_files)) {
		/* No files?  No score, but we "pass". */
		build.total_score = 0;
		return NULL;
	}
	return run_command(m, "ld -r -o ../%s.o %s", m->basename, obj_list(m));
}

static const char *describe_build(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "The object file for the module didn't build:\n"
			       "%s", (char *)check_result);
}

struct ccanlint build = {
	.name = "Module can be built",
	.total_score = 1,
	.check = do_build,
	.describe = describe_build,
	.can_run = can_build,
};

REGISTER_TEST(build, &depends_built, NULL);
