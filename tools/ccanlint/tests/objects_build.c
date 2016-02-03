#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/tal/path/path.h>
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
#include "build.h"

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static char *cflags_list(const struct manifest *m)
{
	unsigned int i;
	char *ret = tal_strdup(m, cflags);

	char **flags = get_cflags(m, m->dir, get_or_compile_info);
	for (i = 0; flags[i]; i++)
		tal_append_fmt(&ret, " %s", flags[i]);
	return ret;
}

void build_objects(struct manifest *m,
		   struct score *score, const char *flags,
		   enum compile_type ctype)
{
	struct ccan_file *i;
	bool errors = false, warnings = false;

	if (list_empty(&m->c_files))
		score->total = 0;
	else
		score->total = 2;

	list_for_each(&m->c_files, i, list) {
		char *output;
		char *fullfile = path_join(m, m->dir, i->name);

		i->compiled[ctype] = temp_file(m, "", fullfile);
		if (!compile_object(score, fullfile, ccan_dir, compiler, flags,
				    i->compiled[ctype], &output)) {
			tal_free(i->compiled[ctype]);
			score_file_error(score, i, 0,
					 "Compiling object files:\n%s",
					 output);
			errors = true;
		} else if (!streq(output, "")) {
			score_file_error(score, i, 0,
					 "Compiling object files gave"
					 " warnings:\n%s",
					 output);
			warnings = true;
		}
	}

	if (!errors) {
		score->pass = true;
		score->score = score->total - warnings;
	} else
		build_failed = true;
}

static void check_objs_build(struct manifest *m,
			     unsigned int *timeleft, struct score *score)
{
	const char *flags;

	flags = cflags_list(m);
	build_objects(m, score, flags, COMPILE_NORMAL);
}

struct ccanlint objects_build = {
	.key = "objects_build",
	.name = "Module object files can be built",
	.compulsory = true,
	.check = check_objs_build,
	.can_run = can_build,
	.needs = "depends_exist info_ported"
};

REGISTER_TEST(objects_build);
