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

static bool has_dep(struct manifest *m, const char *depname)
{
	struct manifest *i;

	/* We can include ourselves, of course. */
	if (streq(depname, m->basename))
		return true;

	list_for_each(&m->deps, i, list) {
		if (streq(i->basename, depname))
			return true;
	}
	return false;
}

static void check_depends_accurate(struct manifest *m,
				   bool keep,
				   unsigned int *timeleft, struct score *score)
{
	struct list_head *list;

	foreach_ptr(list, &m->c_files, &m->h_files,
		    &m->run_tests, &m->api_tests,
		    &m->compile_ok_tests, &m->compile_fail_tests,
		    &m->other_test_c_files) {
		struct ccan_file *f;

		list_for_each(list, f, list) {
			unsigned int i;
			char **lines = get_ccan_file_lines(f);

			for (i = 0; lines[i]; i++) {
				char *mod;
				if (!strreg(f, lines[i],
					    "^[ \t]*#[ \t]*include[ \t]*[<\"]"
					    "ccan/+([^/]+)/", &mod))
					continue;
				if (has_dep(m, mod))
					continue;
				score->error = "Includes a ccan module"
					" not listed in _info";
				score_file_error(score, f, i+1, lines[i]);
			}
		}
	}

	if (!score->error) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint depends_accurate = {
	.key = "depends_accurate",
	.name = "Module's CCAN dependencies are the only CCAN files #included",
	.check = check_depends_accurate,
	.needs = "depends_exist"
};

REGISTER_TEST(depends_accurate);
