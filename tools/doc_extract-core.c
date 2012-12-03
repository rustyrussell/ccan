/* This merely extracts, doesn't do XML or anything. */
#include <ccan/take/take.h>
#include <ccan/str/str.h>
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

static char **grab_doc(char **lines, unsigned int **linemap,
		       const char *file)
{
	char **ret;
	unsigned int i, num;
	bool printing = false;

	ret = tal_arr(NULL, char *, tal_count(lines));
	*linemap = tal_arr(ret, unsigned int, tal_count(lines));

	num = 0;
	for (i = 0; lines[i]; i++) {
		if (streq(lines[i], "/**")) {
			printing = true;
			if (num != 0) {
				ret[num-1] = tal_strcat(NULL,
							take(ret[num-1]), "\n");
			}
		} else if (streq(lines[i], " */")) 
			printing = false;
		else if (printing) {
			if (strstarts(lines[i], " * "))
				ret[num++] = tal_strdup(ret, lines[i]+3);
			else if (strstarts(lines[i], " *"))
				ret[num++] = tal_strdup(ret, lines[i]+2);
			else {
				/* Weird, malformed? */
				static bool warned;
				if (!warned) {
					warnx("%s:%u:"
					      " Expected ' *' in comment.",
					      file, i+1);
					warned++;
				}
				ret[num++] = tal_strdup(ret, lines[i]);
				if (strstr(lines[i], "*/"))
					printing = false;
			}
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
	if (!tal_strreg(ctx, line,
		    "^([A-Z][a-zA-Z0-9_]*( [A-Z][a-zA-Z0-9_]*)*):[ \t\n]*(.*)",
		    &secname, NULL, value))
		return NULL;

	return secname;
}

/* Summary line is form '<identifier> - ' (spaces for 'struct foo -') */
static unsigned int is_summary_line(const char *line)
{
	unsigned int id_len;

	/* We allow /, because it can be in (nested) module names. */
	id_len = strspn(line, IDENT_CHARS" /");
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
		tal_free(d);
	}

	d = tal(list, struct doc_section);
	d->function = function;
	lowertype = tal_arr(d, char, strlen(type) + 1);
	/* Canonicalize type to lower case. */
	for (i = 0; i < strlen(type)+1; i++)
		lowertype[i] = tolower(type[i]);
	d->type = lowertype;
	d->lines = tal_arr(d, char *, 0);
	d->num_lines = 0;
	d->srcline = srcline;

	list_add_tail(list, &d->list);
	return d;
}

static void add_line(struct doc_section *curr, const char *line)
{
	char *myline = tal_strdup(curr->lines, line);
	tal_expand(&curr->lines, &myline, 1);
	curr->num_lines++;
}

/* We convert tabs to spaces here. */
static void add_detabbed_line(struct doc_section *curr, const char *rawline)
{
	unsigned int i, eff_i, len, off = 0;
	char *line;

	/* Worst-case alloc: 8 spaces per tab. */
	line = tal_arr(curr, char, strlen(rawline) +
		       strcount(rawline, "\t") * 7 + 1);
	len = 0;

	/* We keep track of the *effective* offset of i. */
	for (i = eff_i = 0; i < strlen(rawline); i++) {
		if (rawline[i] == '\t') {
			do {
				line[len++] = ' ';
				eff_i++;
			} while (eff_i % 8 != 0);
		} else {
			line[len++] = rawline[i];
			if (off == 0 && rawline[i] == '*')
				off = i + 1;
			eff_i++;
		}
	}
	line[len] = '\0';

	add_line(curr, line + off);
	tal_free(line);
}

/* Not very efficient: we could track prefix length while doing
 * add_detabbed_line */
static void trim_lines(struct doc_section *curr)
{
	unsigned int i, trim = -1;
	int last_non_empty = -1;

	/* Get minimum whitespace prefix. */
	for (i = 0; i < curr->num_lines; i++) {
		unsigned int prefix = strspn(curr->lines[i], " ");
		/* Ignore blank lines */
		if (curr->lines[i][prefix] == '\0')
			continue;
		if (prefix < trim)
			trim = prefix;
	}

	/* Now trim it. */
	for (i = 0; i < curr->num_lines; i++) {
		unsigned int prefix = strspn(curr->lines[i], " ");
		if (prefix < trim)
			curr->lines[i] += prefix;
		else
			curr->lines[i] += trim;

		/* All blank?  Potential to trim. */
		if (curr->lines[i][strspn(curr->lines[i], " \t")] != '\0')
			last_non_empty = i;
	}

	/* Remove trailing blank lines. */
	curr->num_lines = last_non_empty + 1;
}

struct list_head *extract_doc_sections(char **rawlines, const char *file)
{
	unsigned int *linemap;
	char **lines = grab_doc(rawlines, &linemap, file);
	const char *function = NULL;
	struct doc_section *curr = NULL;
	unsigned int i;
	struct list_head *list;

	list = tal(NULL, struct list_head);
	list_head_init(list);

	for (i = 0; lines[i]; i++) {
		unsigned funclen;
		char *type, *extra;

		funclen = is_summary_line(lines[i]);
		if (funclen) {
			function = tal_strndup(list, lines[i], funclen);
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
				add_detabbed_line(curr, rawlines[linemap[i]]);
		}
	}

	list_for_each(list, curr, list)
		trim_lines(curr);

	tal_free(lines);
	return list;
}
