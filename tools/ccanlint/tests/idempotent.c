#include <tools/ccanlint/ccanlint.h>
#include <tools/tools.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
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

static const char explain[] 
= "Headers usually start with the C preprocessor lines to prevent multiple\n"
  "inclusions.  These look like the following:\n"
  "#ifndef MY_HEADER_H\n"
  "#define MY_HEADER_H\n"
  "...\n"
  "#endif /* MY_HEADER_H */\n";

static char *report_idem(struct ccan_file *f, char *sofar)
{
	struct line_info *line_info;
	unsigned int i, first_preproc_line;
	const char *line, *sym;

	line_info = get_ccan_line_info(f);
	if (f->num_lines < 3)
		/* FIXME: We assume small headers probably uninteresting. */
		return sofar;

	for (i = 0; i < f->num_lines; i++) {
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (line_info[i].type == CODE_LINE)
			return talloc_asprintf_append(sofar,
			      "%s:%u:expect first non-comment line to be #ifndef.\n", f->name, i+1);
		else if (line_info[i].type == PREPROC_LINE)
			break;
	}

	/* No code at all?  Don't complain. */
	if (i == f->num_lines)
		return sofar;

	first_preproc_line = i;
	for (i = first_preproc_line+1; i < f->num_lines; i++) {
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (line_info[i].type == CODE_LINE)
			return talloc_asprintf_append(sofar,
			      "%s:%u:expect second line to be #define.\n", f->name, i+1);
		else if (line_info[i].type == PREPROC_LINE)
			break;
	}

	/* No code at all?  Weird. */
	if (i == f->num_lines)
		return sofar;

	/* We expect a condition on this line. */
	if (!line_info[i].cond) {
		return talloc_asprintf_append(sofar,
					      "%s:%u:expected #ifndef.\n",
					      f->name, first_preproc_line+1);
	}

	line = f->lines[i];

	/* We expect the condition to be ! IFDEF <symbol>. */
	if (line_info[i].cond->type != PP_COND_IFDEF
	    || !line_info[i].cond->inverse) {
		return talloc_asprintf_append(sofar,
					      "%s:%u:expected #ifndef.\n",
					      f->name, first_preproc_line+1);
	}

	/* And this to be #define <symbol> */
	if (!get_token(&line, "#"))
		abort();
	if (!get_token(&line, "define")) {
		return talloc_asprintf_append(sofar,
			      "%s:%u:expected '#define %s'.\n",
			      f->name, i+1, line_info[i].cond->symbol);
	}
	sym = get_symbol_token(f, &line);
	if (!sym || !streq(sym, line_info[i].cond->symbol)) {
		return talloc_asprintf_append(sofar,
			      "%s:%u:expected '#define %s'.\n",
			      f->name, i+1, line_info[i].cond->symbol);
	}

	/* Rest of code should all be covered by that conditional. */
	for (i++; i < f->num_lines; i++) {
		unsigned int val = 0;
		if (line_info[i].type == DOC_LINE
		    || line_info[i].type == COMMENT_LINE)
			continue;
		if (get_ccan_line_pp(line_info[i].cond, sym, &val, NULL)
		    != NOT_COMPILED)
			return talloc_asprintf_append(sofar,
			      "%s:%u:code outside idempotent region.\n",
			      f->name, i+1);
	}

	return sofar;
}

static void *check_idempotent(struct manifest *m,
			      bool keep,
			      unsigned int *timeleft)
{
	struct ccan_file *f;
	char *report = NULL;

	list_for_each(&m->h_files, f, list)
		report = report_idem(f, report);

	return report;
}

static const char *describe_idempotent(struct manifest *m, void *check_result)
{
	return talloc_asprintf(check_result, 
			       "Some headers not idempotent:\n"
			       "%s\n%s", (char *)check_result,
			       explain);
}

struct ccanlint idempotent = {
	.key = "idempotent",
	.name = "Module headers are #ifndef/#define wrapped",
	.total_score = 1,
	.check = check_idempotent,
	.describe = describe_idempotent,
};

REGISTER_TEST(idempotent, NULL);
