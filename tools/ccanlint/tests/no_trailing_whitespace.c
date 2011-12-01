/* Trailing whitespace test.  Almost embarrassing, but trivial. */
#include <tools/ccanlint/ccanlint.h>
#include <ccan/talloc/talloc.h>
#include <ccan/foreach/foreach.h>
#include <ccan/str/str.h>

/* FIXME: only print full analysis if verbose >= 2.  */
static char *get_trailing_whitespace(const char *line)
{
	const char *e = strchr(line, 0);
	while (e>line && (e[-1]==' ' || e[-1]=='\t'))
		e--;
	if (*e == 0)
		return NULL; //there were no trailing spaces
	if (e == line)
		return NULL; //the line only consists of spaces

	if (strlen(line) > 20)
		return talloc_asprintf(line, "...'%s'",
				       line + strlen(line) - 20);
	return talloc_asprintf(line, "'%s'", line);
}

static void check_trailing_whitespace(struct manifest *m,
				      bool keep,
				      unsigned int *timeleft,
				      struct score *score)
{
	struct list_head *list;
	struct ccan_file *f;
	unsigned int i;

	/* We don't fail ccanlint for this. */
	score->pass = true;

	foreach_ptr(list, &m->c_files, &m->h_files) {
		list_for_each(list, f, list) {
			char **lines = get_ccan_file_lines(f);
			for (i = 0; i < f->num_lines; i++) {
				char *err = get_trailing_whitespace(lines[i]);
				if (err)
					score_file_error(score, f, i+1,
							 "%s", err);
			}
		}
	}
	if (!score->error) {
		score->score = score->total;
	}
}

struct ccanlint no_trailing_whitespace = {
	.key = "no_trailing_whitespace",
	.name = "Module's source code has no trailing whitespace",
	.check = check_trailing_whitespace,
	.needs = "info_exists"
};


REGISTER_TEST(no_trailing_whitespace);
