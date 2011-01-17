#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <stdio.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>

/* Summary line is form '<identifier> - ' (spaces for 'struct foo -') */
/* slightly modified from doc_extract-core.c */
static unsigned int is_summary_line(const char *line)
{
	unsigned int id_len;

	id_len = strspn(line, IDENT_CHARS" *");
	if (id_len == 0)
		return 0;
	if (strspn(line, " ") == id_len)
		return 0;
	if (!strstarts(line + id_len-1, " - "))
		return 0;
	return id_len - 1;
}

static void check_info_summary_single_line(struct manifest *m,
					   bool keep,
					   unsigned int *timeleft,
					   struct score *score)
{
	int i = 0;
	get_ccan_line_info(m->info_file);
	score->total = 1;
	for (i = 0; i < m->info_file->num_lines; ++i) {
		if (is_summary_line(m->info_file->lines[i])) {
			if (strspn(m->info_file->lines[i+1], " *") == strlen(m->info_file->lines[i+1])) {
				/* valid summary line */
				score->error = NULL;
				score->pass = true;
				score->score = 1;
			} else {
				/* invalid summary line - line following summary line should be empty */
				score->pass = false;
				score->score = 0;
				score->error = "invalid summary line - not on a single line:";
				score_file_error(score, m->info_file, i+1, "summary is not on a single line");
			}
			break;
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
