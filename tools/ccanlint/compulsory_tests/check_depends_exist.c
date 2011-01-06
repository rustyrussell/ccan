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

static bool add_dep(struct manifest *m, const char *dep, struct score *score)
{
	struct stat st;
	struct manifest *subm;
	char *dir = talloc_asprintf(m, "%s/%s", ccan_dir, dep);

	/* FIXME: get_manifest has a tendency to exit. */
	if (stat(dir, &st) != 0) {
		score->error
			= talloc_asprintf(m,
					  "Could not stat dependency %s: %s",
					  dir, strerror(errno));
		return false;
	}
	subm = get_manifest(m, dir);
	list_add_tail(&m->deps, &subm->list);
	return true;
}

static void check_depends_exist(struct manifest *m,
				bool keep,
				unsigned int *timeleft, struct score *score)
{
	unsigned int i;
	char **deps;
	char *updir = talloc_strdup(m, m->dir);

	*strrchr(updir, '/') = '\0';

	if (safe_mode)
		deps = get_safe_ccan_deps(m, m->dir, true,
					  &m->info_file->compiled);
	else
		deps = get_deps(m, m->dir, true, &m->info_file->compiled);

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/"))
			continue;

		if (!add_dep(m, deps[i], score))
			return;
	}

	/* We may need libtap for testing, unless we're "tap" */
	if (!streq(m->basename, "tap")
	    && (!list_empty(&m->run_tests) || !list_empty(&m->api_tests))) {
		if (!add_dep(m, "ccan/tap", score))
			return;
	}

	score->pass = true;
	score->score = score->total;
}

struct ccanlint depends_exist = {
	.key = "depends-exist",
	.name = "Module's CCAN dependencies are present",
	.check = check_depends_exist,
};

REGISTER_TEST(depends_exist, &has_info, NULL);
