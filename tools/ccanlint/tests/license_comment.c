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
#include <ccan/tal/str/str.h>

/**
 * line_has_license_flavour - returns true if line contains a <flavour> license
 * @line: line to look for license in
 * @shortname: license to find
 * @note ("LGPLv2.0","LGPL") returns true
 * @note ("LGPLv2.0","GPL") returns false
 */
static bool line_has_license_flavour(const char *line, const char *shortname)
{
	char **toks = tal_strsplit(NULL, line, " \t-:", STR_NO_EMPTY);
	size_t i;
	bool ret = false;

	for (i = 0; toks[i] != NULL; i++) {
		if (strstarts(toks[i], shortname)) {
			ret = true;
			break;
		}
	}
	tal_free(toks);
	return ret;
}

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
				if (line_has_license_flavour(lines[i],
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

static void add_license_comment(struct manifest *m, struct score *score)
{
	struct file_error *e;
	const char *license_desc = get_license_oneliner(score, m->license);
	char *files = tal_strdup(score, ""), *q;

	list_for_each(&score->per_file_errors, e, list)
		tal_append_fmt(&files, "  %s\n", e->file->name);

	q = tal_fmt(score, "The following files don't have a comment:\n%s\n"
		    "Should I prepend '%s'?", files, license_desc);
	if (!ask(q))
		return;

	list_for_each(&score->per_file_errors, e, list) {
		char *tmpname;
		FILE *out;
		unsigned int i;

		tmpname = temp_file(score, ".licensed", e->file->name);
		out = fopen(tmpname, "w");
		if (!out)
			err(1, "Opening %s", tmpname);
		if (fprintf(out, "%s\n", license_desc) < 0)
			err(1, "Writing %s", tmpname);

		for (i = 0; e->file->lines[i]; i++)
			if (fprintf(out, "%s\n", e->file->lines[i]) < 0)
				err(1, "Writing %s", tmpname);

		if (fclose(out) != 0)
			err(1, "Closing %s", tmpname);

		if (!move_file(tmpname, e->file->fullname))
			err(1, "Moving %s to %s", tmpname, e->file->fullname);
	}
}

struct ccanlint license_comment = {
	.key = "license_comment",
	.name = "Source and header files refer to LICENSE",
	.check = check_license_comment,
	.handle = add_license_comment,
	.needs = "license_exists"
};
REGISTER_TEST(license_comment);
