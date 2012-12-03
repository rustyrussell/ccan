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
#include <ccan/str/str.h>

static void check_license_file_compat(struct manifest *m,
				      unsigned int *timeleft,
				      struct score *score)
{
	struct list_head *list;

	/* FIXME: Ignore unknown licenses for now. */
	if (m->license == LICENSE_UNKNOWN) {
		score->pass = true;
		score->score = score->total = 0;
		return;
	}

	foreach_ptr(list, &m->c_files, &m->h_files) {
		struct ccan_file *f;

		list_for_each(list, f, list) {
			enum license l;

			/* Check they don't have boilerplate for incompatible
			 * license! */
			for (l = 0; l < LICENSE_UNKNOWN; l++) {
				if (!find_boilerplate(f, l))
					continue;
				if (license_compatible[m->license][l])
					break;
				score_file_error(score, f, 0,
						 "Found boilerplate for license '%s' which is incompatible with '%s'",
						 licenses[l].name,
						 licenses[m->license].name);
				break;
			}
		}
	}

	if (list_empty(&score->per_file_errors)) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint license_file_compat = {
	.key = "license_file_compat",
	.name = "Source files don't contain incompatible licenses",
	.check = check_license_file_compat,
	.needs = "license_exists"
};
REGISTER_TEST(license_file_compat);
