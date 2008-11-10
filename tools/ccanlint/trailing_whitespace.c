/* Trailing whitespace test.  Almost embarrassing, but trivial. */
#include "ccanlint.h"
#include <talloc/talloc.h>
#include <str/str.h>

static char *report_on_trailing_whitespace(const char *line)
{
	if (!strends(line, " ") && !strends(line, "\t"))
		return NULL;

	if (strlen(line) > 20)
		return talloc_asprintf(line, "...'%s'",
				       line + strlen(line) - 20);
	return talloc_asprintf(line, "'%s'", line);
}

static void *check_trailing_whitespace(struct manifest *m)
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
	.name = "Lines with unnecessary trailing whitespace",
	.total_score = 1,
	.check = check_trailing_whitespace,
	.describe = describe_trailing_whitespace,
};
