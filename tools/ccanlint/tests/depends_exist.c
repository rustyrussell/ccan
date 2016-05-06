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

static bool have_dep(struct manifest *m, const char *dep)
{
	struct manifest *i;

	list_for_each(&m->deps, i, list)
		if (streq(i->modname, dep + strlen("ccan/")))
			return true;

	return false;
}

static bool add_dep(struct manifest *m,
		    struct list_head *deplist,
		    const char *dep, struct score *score)
{
	struct stat st;
	struct manifest *subm;
	char *dir = path_join(m, ccan_dir, dep);

	/* FIXME: get_manifest has a tendency to exit. */
	if (stat(dir, &st) != 0) {
		score->error = tal_fmt(m, "Could not stat dependency %s: %s",
				       dir, strerror(errno));
		return false;
	}
	subm = get_manifest(m, dir);
	list_add_tail(deplist, &subm->list);
	return true;
}

/* FIXME: check this is still true once we reduce features. */
static void check_depends_exist(struct manifest *m,
				unsigned int *timeleft, struct score *score)
{
	unsigned int i;
	char **deps;

	if (safe_mode)
		deps = get_safe_ccan_deps(m, m->dir, "depends", true);
	else
		deps = get_deps(m, m->dir, "depends", true,
				get_or_compile_info);

	if (!deps) {
		score->error = tal_fmt(m, "Could not extract dependencies");
		return;
	}

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/")) {
			non_ccan_deps = true;
			continue;
		}

		if (!add_dep(m, &m->deps, deps[i], score))
			return;
	}

	score->pass = true;
	score->score = score->total;
}

static void check_test_depends_exist(struct manifest *m,
				     unsigned int *timeleft,
				     struct score *score)
{
	unsigned int i;
	char **deps;
	bool needs_tap;

	/* We may need libtap for testing, unless we're "tap" */
	if (streq(m->modname, "tap")) {
		needs_tap = false;
	} else if (list_empty(&m->run_tests) && list_empty(&m->api_tests)) {
		needs_tap = false;
	} else {
		needs_tap = true;
	}

	if (safe_mode)
		deps = get_safe_ccan_deps(m, m->dir, "testdepends", true);
	else
		deps = get_deps(m, m->dir, "testdepends", true,
				get_or_compile_info);

	for (i = 0; deps[i]; i++) {
		if (!strstarts(deps[i], "ccan/"))
			continue;

		/* Don't add dependency twice: we can only be on one list!
		 * Also, it's possible to have circular test depends, so drop
		 * self-refs. */
		if (!have_dep(m, deps[i])
		    && !streq(deps[i] + strlen("ccan/"), m->modname)
		    && !add_dep(m, &m->test_deps, deps[i], score))
			return;

		if (streq(deps[i], "ccan/tap")) {
			needs_tap = false;
		}
	}

	if (needs_tap && !have_dep(m, "ccan/tap")
	    && !add_dep(m, &m->test_deps, "ccan/tap", score)) {
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
	.needs = "info_compiles"
};

REGISTER_TEST(depends_exist);

struct ccanlint test_depends_exist = {
	.key = "test_depends_exist",
	.name = "Module's CCAN test dependencies can be found",
	.compulsory = false,
	.check = check_test_depends_exist,
	.needs = "depends_exist"
};

REGISTER_TEST(test_depends_exist);
