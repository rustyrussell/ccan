#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/foreach/foreach.h>
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
#include "build.h"

static const char *can_build(struct manifest *m)
{
	if (safe_mode)
		return "Safe mode enabled";
	return NULL;
}

static bool expect_obj_file(struct manifest *m)
{
	/* If it has C files, we expect an object file built from them. */
	return !list_empty(&m->c_files);
}

static char *build_subdir_objs(struct manifest *m,
			       const char *flags,
			       enum compile_type ctype)
{
	struct ccan_file *i;

	list_for_each(&m->c_files, i, list) {
		char *fullfile = tal_fmt(m, "%s/%s", m->dir, i->name);
		char *output;

		i->compiled[ctype] = temp_file(m, "", fullfile);
		if (!compile_object(m, fullfile, ccan_dir, compiler, flags,
				    i->compiled[ctype], &output)) {
			tal_free(i->compiled[ctype]);
			i->compiled[ctype] = NULL;
			return tal_fmt(m,
				       "Dependency %s did not build:\n%s",
				       m->modname, output);
		}
	}
	return NULL;
}

char *build_submodule(struct manifest *m, const char *flags,
		      enum compile_type ctype)
{
	char *errstr;

	if (m->compiled[ctype])
		return NULL;

	if (!expect_obj_file(m))
		return NULL;

	if (verbose >= 2)
		printf("  Building dependency %s\n", m->dir);

	errstr = build_subdir_objs(m, flags, ctype);
	if (errstr)
		return errstr;

	m->compiled[ctype] = build_module(m, ctype, &errstr);
	if (!m->compiled[ctype])
		return errstr;
	return NULL;
}

static void check_depends_built(struct manifest *m,
				unsigned int *timeleft, struct score *score)
{
	struct list_head *list;

	foreach_ptr(list, &m->deps, &m->test_deps) {
		struct manifest *i;
		list_for_each(list, i, list) {
			char *errstr;

			errstr = build_submodule(i, cflags, COMPILE_NORMAL);

			if (errstr) {
				score->error = tal_fmt(score,
						       "Dependency %s"
						       " did not"
						       " build:\n%s",
						       i->modname,
						       errstr);
				return;
			}
		}
	}

	score->pass = true;
	score->score = score->total;
}

struct ccanlint depends_build = {
	.key = "depends_build",
	.name = "Module's CCAN dependencies can be found or built",
	.check = check_depends_built,
	.can_run = can_build,
	.needs = "depends_exist test_depends_exist info_ported"
};

REGISTER_TEST(depends_build);
