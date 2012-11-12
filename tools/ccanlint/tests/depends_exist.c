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

/* FIXME: check this is still true once we reduce features. */
static void check_depends_exist(struct manifest *m,
				unsigned int *timeleft, struct score *score)
{
	unsigned int i;
	char **deps;
	char *updir = talloc_strdup(m, m->dir);
	bool needs_tap;

	if (strrchr(updir, '/'))
		*strrchr(updir, '/') = '\0';

	/* We may need libtap for testing, unless we're "tap" */
	if (streq(m->basename, "tap")) {
		needs_tap = false;
	} else if (list_empty(&m->run_tests) && list_empty(&m->api_tests)) {
		needs_tap = false;
	} else {
		needs_tap = true;
	}

	if (safe_mode)
		deps = get_safe_ccan_deps(m, m->dir, "depends", true);
	else
		deps = get_deps(m, m->dir, "depends", true,
				get_or_compile_info);

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/"))
			continue;

		if (!add_dep(m, deps[i], score))
			return;

		if (streq(deps[i], "ccan/tap")) {
			needs_tap = false;
		}
	}

	if (needs_tap && !add_dep(m, "ccan/tap", score)) {
		return;
	}

	score->pass = true;
	score->score = score->total;
}

struct ccanlint depends_exist = {
	.key = "depends_exist",
	.name = "Module's CCAN dependencies can be found",
	.compulsory = true,
	.check = check_depends_exist,
	.needs = "info_exists"
};

REGISTER_TEST(depends_exist);
