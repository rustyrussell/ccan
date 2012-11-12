#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/foreach/foreach.h>
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

static bool has_dep(struct manifest *m, bool test_depend, const char *depname)
{
	struct manifest *i;

	/* We can include ourselves, of course. */
	if (streq(depname, m->basename))
		return true;

	list_for_each(&m->deps, i, list) {
		if (streq(i->basename, depname))
			return true;
	}

	if (test_depend) {
		list_for_each(&m->test_deps, i, list) {
			if (streq(i->basename, depname))
				return true;
		}
	}

	return false;
}

static void check_dep_includes(struct manifest *m, struct score *score,
			       struct ccan_file *f, bool test_depend)
{
	unsigned int i;
	char **lines = get_ccan_file_lines(f);
	struct line_info *li = get_ccan_line_info(f);

	for (i = 0; lines[i]; i++) {
		char *mod;
		if (!strreg(f, lines[i],
			    "^[ \t]*#[ \t]*include[ \t]*[<\"]"
			    "ccan/+([^/]+)/", &mod))
			continue;

		if (has_dep(m, test_depend, mod))
			continue;

		/* FIXME: we can't be sure about
		 * conditional includes, so don't
		 * complain. */
		if (!li[i].cond) {
			score_file_error(score, f, i+1,
					 "%s not listed in _info", mod);
		}
	}
}

static void check_depends_accurate(struct manifest *m,
				   unsigned int *timeleft, struct score *score)
{
	struct list_head *list;

	foreach_ptr(list, &m->c_files, &m->h_files) {
		struct ccan_file *f;

		list_for_each(list, f, list)
			check_dep_includes(m, score, f, false);
	}

	foreach_ptr(list, &m->run_tests, &m->api_tests,
		    &m->compile_ok_tests, &m->compile_fail_tests,
		    &m->other_test_c_files) {
		struct ccan_file *f;

		list_for_each(list, f, list)
			check_dep_includes(m, score, f, true);
	}

	if (!score->error) {
		score->score = score->total;
		score->pass = true;
	}
}

struct ccanlint depends_accurate = {
	.key = "depends_accurate",
	.name = "Module's CCAN dependencies are the only CCAN files #included",
	.check = check_depends_accurate,
	.needs = "depends_exist test_depends_exist"
};

REGISTER_TEST(depends_accurate);
