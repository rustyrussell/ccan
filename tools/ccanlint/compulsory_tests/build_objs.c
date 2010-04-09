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

static void *check_objs_build(struct manifest *m)
{
	char *report = NULL;
	struct ccan_file *i;

	list_for_each(&m->c_files, i, list) {
		char *err;
		char *fullfile = talloc_asprintf(m, "%s/%s", m->dir, i->name);

		/* One point for each obj file. */
		build_objs.total_score++;

		i->compiled = compile_object(m, fullfile, ccan_dir, &err);
		if (!i->compiled) {
			if (report)
				report = talloc_append_string(report, err);
			else
				report = err;
		}
	}
	return report;
}

static const char *describe_objs_build(struct manifest *m, void *check_result)
{
	return check_result;
}

struct ccanlint build_objs = {
	.key = "build-objs",
	.name = "Module object files can be built",
	.check = check_objs_build,
	.describe = describe_objs_build,
	.can_run = can_build,
};

REGISTER_TEST(build_objs, &depends_exist, NULL);
