#include "ccanlint.h"
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
#include "../tools.h"

static const char explain[] 
= "Headers usually start with the C preprocessor lines to prevent multiple\n"
  "inclusions.  These look like the following:\n"
  "#ifndef MY_HEADER_H\n"
  "#define MY_HEADER_H\n"
  "...\n"
  "#endif /* MY_HEADER_H */\n";

static char *get_ifndef_sym(char *line)
{
	line += strspn(line, SPACE_CHARS);
	if (line[0] == '#')
	{
		line++;
		line += strspn(line, SPACE_CHARS);
		if (strstarts(line, "ifndef") && isspace(line[6]))
			return line+6+strspn(line+6, SPACE_CHARS);
		else if (strstarts(line, "if"))
		{
			line += 2;
			line += strspn(line, SPACE_CHARS);
			if (line[0] == '!')
			{
				line++;
				line += strspn(line, SPACE_CHARS);
				if (strstarts(line, "defined"))
				{
					line += 7;
					line += strspn(line, SPACE_CHARS);
					if (line[0] == '(')
					{
						line++;
						line += strspn(line,
							SPACE_CHARS);
					}
					return line;
				}
			}
		}
	}
	return NULL;
}

static int is_define(char *line, char *id, size_t id_len)
{
	line += strspn(line, SPACE_CHARS);
	if (line[0] == '#')
	{
		line++;
		line += strspn(line, SPACE_CHARS);
		if (strstarts(line, "define") && isspace(line[6]))
		{
			line += 6;
			line += strspn(line, SPACE_CHARS);
			if (strspn(line, IDENT_CHARS) == id_len &&
			    memcmp(id, line, id_len) == 0)
				return 1;
		}
	}
	return 0;
}

static char *report_idem(struct ccan_file *f, char *sofar)
{
	char **lines;
	char *id;
	size_t id_len;

	lines = get_ccan_file_lines(f);
	if (f->num_lines < 3)
		/* FIXME: We assume small headers probably uninteresting. */
		return NULL;

	id = get_ifndef_sym(lines[0]);
	if (!id)
		return talloc_asprintf_append(sofar,
			"%s:1:expect first line to be #ifndef.\n", f->name);
	id_len = strspn(id, IDENT_CHARS);

	if (!is_define(lines[1], id, id_len))
		return talloc_asprintf_append(sofar,
			"%s:2:expect second line to be '#define %.*s'.\n",
			f->name, (int)id_len, id);

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
