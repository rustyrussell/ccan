/* This merely extracts, doesn't do XML or anything. */
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ctype.h>
#include "doc_extract.h"
#include "tools.h"

static char **grab_doc(char **lines, unsigned int **linemap)
{
	char **ret;
	unsigned int i, num;
	bool printing = false;

	ret = talloc_array(NULL, char *, talloc_array_length(lines));
	*linemap = talloc_array(ret, unsigned int, talloc_array_length(lines));

	num = 0;
	for (i = 0; lines[i]; i++) {
		if (streq(lines[i], "/**")) {
			printing = true;
			if (num != 0) {
				ret[num-1] = talloc_append_string(ret[num-1],
								  "\n");
			}
		} else if (streq(lines[i], " */")) 
			printing = false;
		else if (printing) {
			if (strstarts(lines[i], " * "))
				ret[num++] = talloc_strdup(ret, lines[i]+3);
			else if (strstarts(lines[i], " *"))
				ret[num++] = talloc_strdup(ret, lines[i]+2);
			else
				errx(1, "Malformed line %u", i);
			(*linemap)[num-1] = i;
		}
	}
	ret[num] = NULL;
	return ret;
}

static bool is_blank(const char *line)
{
	return line && line[strspn(line, " \t\n")] == '\0';
}

static char *is_section(const void *ctx, const char *line, char **value)
{
	char *secname;

	/* Any number of upper case words separated by spaces, ending in : */
	if (!strreg(ctx, line,
		    "^([A-Z][a-zA-Z0-9_]*( [A-Z][a-zA-Z0-9_]*)*):[ \t\n]*(.*)",
		    &secname, NULL, value))
		return NULL;

	return secname;
}

/* Summary line is form '<identifier> - ' (spaces for 'struct foo -') */
static unsigned int is_summary_line(const char *line)
{
	unsigned int id_len;

	id_len = strspn(line, IDENT_CHARS" ");
	if (id_len == 0)
		return 0;
	if (strspn(line, " ") == id_len)
		return 0;
	if (!strstarts(line + id_len-1, " - "))
		return 0;
	return id_len - 1;
}

static bool empty_section(struct doc_section *d)
{
	unsigned int i;

	for (i = 0; i < d->num_lines; i++)
		if (!is_blank(d->lines[i]))
			return false;
	return true;
}

static struct doc_section *new_section(struct list_head *list,
				       const char *function,
				       const char *type,
				       unsigned int srcline)
{
	struct doc_section *d;
	char *lowertype;
	unsigned int i;

	/* If previous section was empty, delete it. */
	d = list_tail(list, struct doc_section, list);
	if (d && empty_section(d)) {
		list_del(&d->list);
		talloc_free(d);
	}

	d = talloc(list, struct doc_section);
	d->function = function;
	lowertype = talloc_size(d, strlen(type) + 1);
	/* Canonicalize type to lower case. */
	for (i = 0; i < strlen(type)+1; i++)
		lowertype[i] = tolower(type[i]);
	d->type = lowertype;
	d->lines = NULL;
	d->num_lines = 0;
	d->srcline = srcline;

	list_add_tail(list, &d->list);
	return d;
}

static void add_line(struct doc_section *curr, const char *line)
{
	curr->lines = talloc_realloc(curr, curr->lines, char *,
				     curr->num_lines+1);
	curr->lines[curr->num_lines++] = talloc_strdup(curr->lines, line);
}

struct list_head *extract_doc_sections(char **rawlines)
{
	unsigned int *linemap;
	char **lines = grab_doc(rawlines, &linemap);
	const char *function = NULL;
	struct doc_section *curr = NULL;
	unsigned int i;
	struct list_head *list;

	list = talloc(NULL, struct list_head);
	list_head_init(list);

	for (i = 0; lines[i]; i++) {
		unsigned funclen;
		char *type, *extra;

		funclen = is_summary_line(lines[i]);
		if (funclen) {
			function = talloc_strndup(list, lines[i], funclen);
			curr = new_section(list, function, "summary",
					   linemap[i]);
			add_line(curr, lines[i] + funclen + 3);
			curr = new_section(list, function, "description",
					   linemap[i]);
		} else if ((type = is_section(list, lines[i], &extra)) != NULL){
			curr = new_section(list, function, type, linemap[i]);
			if (!streq(extra, "")) {
				add_line(curr, extra);
				curr = NULL;
			}
		} else {
			if (curr)
				add_line(curr, lines[i]);
		}
	}
	talloc_free(lines);
	return list;
}
