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

static void check_license_comment(struct manifest *m,
				  unsigned int *timeleft, struct score *score)
{
	struct list_head *list;

	/* No requirements on public domain. */
	if (m->license == LICENSE_PUBLIC_DOMAIN
	    || m->license == LICENSE_UNKNOWN) {
		score->pass = true;
		score->score = score->total;
		return;
	}

	foreach_ptr(list, &m->c_files, &m->h_files) {
		struct ccan_file *f;

		list_for_each(list, f, list) {
			unsigned int i;
			char **lines = get_ccan_file_lines(f);
			struct line_info *info = get_ccan_line_info(f);
			bool found_license = false, found_flavor = false;

			for (i = 0; lines[i]; i++) {
				if (info[i].type == CODE_LINE)
					break;
				if (strstr(lines[i], "LICENSE"))
					found_license = true;
				if (strstr(lines[i],
					   licenses[m->license].shortname))
					found_flavor = true;
			}
			if ((!found_license || !found_flavor)
			    && !find_boilerplate(f, m->license)) {
				score_file_error(score, f, lines[i] ? i : 0,
						 "No reference to license"
						 " found");
			}
		}
	}

	if (list_empty(&score->per_file_errors)) {
		score->pass = true;
		score->score = score->total;
	}
}

struct ccanlint license_comment = {
	.key = "license_comment",
	.name = "Source and header files refer to LICENSE",
	.check = check_license_comment,
	.needs = "license_exists"
};
REGISTER_TEST(license_comment);
