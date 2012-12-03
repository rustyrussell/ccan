#include <tools/ccanlint/ccanlint.h>
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

static void check_license_depends_compat(struct manifest *m,
					 unsigned int *timeleft,
					 struct score *score)
{
	struct manifest *i;

	score->pass = true;

	/* If our license is unknown, we can't know the answer. */
	if (m->license == LICENSE_UNKNOWN) {
		score->score = score->total = 0;
		return;
	}

	list_for_each(&m->deps, i, list) {
		struct doc_section *d = find_license_tag(i);
		i->license = which_license(d);

		if (i->license != LICENSE_UNKNOWN
		    && !license_compatible[m->license][i->license]) {
			score_file_error(score, i->info_file, 0,
					 "Dependency ccan/%s has"
					 " incompatible license '%s'",
					 i->modname,
					 licenses[i->license].name);
			score->pass = false;
		}
	}

	if (score->pass)
		score->score = score->total;
}

struct ccanlint license_depends_compat = {
	.key = "license_depends_compat",
	.name = "CCAN dependencies don't contain incompatible licenses",
	.check = check_license_depends_compat,
	.needs = "license_exists depends_exist"
};
REGISTER_TEST(license_depends_compat);
