#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/str/str.h>
#include <ccan/take/take.h>
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

static char *obj_list(const struct manifest *m, enum compile_type ctype)
{
	char *list = tal_strdup(m, "");
	struct ccan_file *i;

	/* Objects from all the C files. */
	list_for_each(&m->c_files, i, list) {
		list = tal_strcat(m, take(list), i->compiled[ctype]);
		list = tal_strcat(m, take(list), " ");
	}
	return list;
}

char *build_module(struct manifest *m,
		   enum compile_type ctype, char **errstr)
{
	return link_objects(m, m->basename, obj_list(m, ctype), errstr);
}

static void do_build(struct manifest *m,
		     unsigned int *timeleft,
		     struct score *score)
{
	char *errstr;

	if (list_empty(&m->c_files)) {
		/* No files?  No score, but we "pass". */
		score->total = 0;
		score->pass = true;
		return;
	}

	m->compiled[COMPILE_NORMAL]
		= build_module(m, COMPILE_NORMAL, &errstr);
	if (!m->compiled[COMPILE_NORMAL]) {
		score_error(score, m->modname,"%s", errstr);
		return;
	}

	score->pass = true;
	score->score = score->total;
}

struct ccanlint module_builds = {
	.key = "module_builds",
	.name = "Module can be built from object files",
	.compulsory = true,
	.check = do_build,
	.can_run = can_build,
	.needs = "objects_build"
};

REGISTER_TEST(module_builds);
