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

static void do_build(struct manifest *m,
		     bool keep,
		     unsigned int *timeleft,
		     struct score *score)
{
	char *filename, *errstr;

	if (list_empty(&m->c_files)) {
		/* No files?  No score, but we "pass". */
		score->total = 0;
		score->pass = true;
		return;
	}

	filename = link_objects(m, m->basename, false, obj_list(m), &errstr);
	if (!filename) {
		score->error = "The object file didn't build";
		score_file_error(score, NULL, 0, errstr);
		return;
	}

	if (keep) {
		char *realname = talloc_asprintf(m, "%s.o", m->dir);
		/* We leave this object file around, all built. */
		if (!move_file(filename, realname))
			err(1, "Renaming %s to %s", filename, realname);
	}
	score->pass = true;
	score->score = score->total;
}

struct ccanlint build = {
	.key = "build",
	.name = "Module can be built from object files",
	.check = do_build,
	.can_run = can_build,
};

REGISTER_TEST(build, &depends_built, NULL);
