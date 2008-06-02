#include "ccanlint.h"
#include <talloc/talloc.h>
#include <string/string.h>
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

static const char explain[] 
= "Headers usually start with the C preprocessor lines to prevent multiple\n"
  "inclusions.  These look like the following:\n"
  "#ifndef MY_HEADER_H\n"
  "#define MY_HEADER_H\n"
  "...\n"
  "#endif /* MY_HEADER_H */\n";

static char *report_idem(struct ccan_file *f, char *sofar)
{
	char **lines;
	char *secondline;

	lines = get_ccan_file_lines(f);
	if (f->num_lines < 3)
		/* FIXME: We assume small headers probably uninteresting. */
		return NULL;

	if (!strstarts(lines[0], "#ifndef "))
		return talloc_asprintf_append(sofar,
			"%s:1:expect first line to be #ifndef.\n", f->name);

	secondline = talloc_asprintf(f, "#define %s",
				     lines[0] + strlen("#ifndef "));
	if (!streq(lines[1], secondline))
		return talloc_asprintf_append(sofar,
			"%s:2:expect second line to be '%s'.\n",
			f->name, secondline);

	return sofar;
}

static void *check_idempotent(struct manifest *m)
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
	.name = "Headers are #ifndef/#define idempotent wrapped",
	.total_score = 1,
	.check = check_idempotent,
	.describe = describe_idempotent,
};
