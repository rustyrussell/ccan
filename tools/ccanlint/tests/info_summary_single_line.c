#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <stdio.h>
#include <ccan/str/str.h>

static void check_info_summary_single_line(struct manifest *m,
					   unsigned int *timeleft,
					   struct score *score)
{
	struct list_head *infodocs = get_ccan_file_docs(m->info_file);
	struct doc_section *d;

	score->pass = true;
	score->score = 1;

	list_for_each(infodocs, d, list) {
		const char *after;

		if (!streq(d->type, "summary"))
			continue;

		/* line following summary line should be empty */
		after = m->info_file->lines[d->srcline+1];
		if (after && strspn(after, " *") != strlen(after)) {
			score->pass = false;
			score->score = 0;
			score_file_error(score, m->info_file, d->srcline+1,
					 "%s\n%s",
					 m->info_file->lines[d->srcline],
					 m->info_file->lines[d->srcline+1]);
		}
	}
}

struct ccanlint info_summary_single_line = {
	.key = "info_summary_single_line",
	.name = "Module has a single line summary in _info",
	.check = check_info_summary_single_line,
	.needs = "info_exists"
};

REGISTER_TEST(info_summary_single_line);
