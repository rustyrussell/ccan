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

static void check_objs_build(struct manifest *m,
			     bool keep,
			     unsigned int *timeleft, struct score *score)
{
	struct ccan_file *i;
	bool errors = false, warnings = false;

	if (list_empty(&m->c_files))
		score->total = 0;
	else
		score->total = 2;

	list_for_each(&m->c_files, i, list) {
		char *output;
		char *fullfile = talloc_asprintf(m, "%s/%s", m->dir, i->name);

		i->compiled = maybe_temp_file(m, "", keep, fullfile);
		if (!compile_object(score, fullfile, ccan_dir, "", i->compiled,
				    &output)) {
			talloc_free(i->compiled);
			score->error = "Compiling object files";
			score_file_error(score, i, 0, output);
			errors = true;
		} else if (!streq(output, "")) {
			score->error = "Compiling object files gave warnings";
			score_file_error(score, i, 0, output);
			warnings = true;
		}
	}

	if (!errors) {
		score->pass = true;
		score->score = score->total - warnings;
	}
}

struct ccanlint build_objs = {
	.key = "objects_build",
	.name = "Module object files can be built",
	.check = check_objs_build,
	.can_run = can_build,
	.needs = "depends_exist"
};

REGISTER_TEST(build_objs);
