/* Trailing whitespace test.  Almost embarrassing, but trivial. */
#include <tools/ccanlint/ccanlint.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>

static char *report_on_trailing_whitespace(const char *line)
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

static void *check_trailing_whitespace(struct manifest *m,
				       bool keep,
				       unsigned int *timeleft)
{
	char *report;

	report = report_on_lines(&m->c_files, report_on_trailing_whitespace,
				 NULL);
	report = report_on_lines(&m->h_files, report_on_trailing_whitespace,
				 report);

	return report;
}

static const char *describe_trailing_whitespace(struct manifest *m,
						void *check_result)
{
	return talloc_asprintf(check_result, 
			       "Some source files have trailing whitespace:\n"
			       "%s", (char *)check_result);
}

struct ccanlint trailing_whitespace = {
	.key = "trailing-whitespace",
	.name = "Module's source code has no trailing whitespace",
	.total_score = 1,
	.check = check_trailing_whitespace,
	.describe = describe_trailing_whitespace,
};


REGISTER_TEST(trailing_whitespace, NULL);
