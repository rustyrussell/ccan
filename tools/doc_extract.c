/* This merely extracts, doesn't do XML or anything. */
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ccan/talloc/talloc.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/grab_file/grab_file.h>
#include "tools.h"

static char **grab_doc(const char *fname)
{
	char *file;
	char **lines, **ret;
	unsigned int i, num;
	bool printing = false;

	file = grab_file(NULL, fname, NULL);
	if (!file)
		err(1, "Reading file %s", fname);
	lines = strsplit(file, file, "\n", &num);
	ret = talloc_array(NULL, char *, num+1);

	num = 0;
	for (i = 0; lines[i]; i++) {
		if (streq(lines[i], "/**")) {
			printing = true;
			if (num != 0)
				talloc_append_string(ret[num-1], "\n");
		} else if (streq(lines[i], " */")) 
			printing = false;
		else if (printing) {
			if (strstarts(lines[i], " * "))
				ret[num++] = talloc_strdup(ret, lines[i]+3);
			else if (strstarts(lines[i], " *"))
				ret[num++] = talloc_strdup(ret, lines[i]+2);
			else
				errx(1, "Malformed line %s:%u", fname, i);
		}
	}
	ret[num] = NULL;
	talloc_free(file);
	return ret;
}

static bool is_blank(const char *line)
{
	return line && line[strspn(line, " \t\n")] == '\0';
}

static bool is_section(const char *line, bool maybe_one_liner)
{
	unsigned int len;

	len = strcspn(line, " \t\n:");
	if (len == 0)
		return false;

	if (line[len] != ':')
		return false;

	/* If it can be a one-liner, a space is sufficient.*/
	if (maybe_one_liner && (line[len+1] == ' ' || line[len+1] == '\t'))
		return true;

	return line[len] == ':' && is_blank(line+len+1);
}

/* Summary line is form '<identifier> - ' */
static bool is_summary_line(const char *line)
{
	unsigned int id_len;

	id_len = strspn(line, IDENT_CHARS);
	if (id_len == 0)
		return false;
	if (!strstarts(line + id_len, " - "))
		return false;

	return true;
}

static bool end_section(const char *line)
{
	return !line || is_section(line, true) || is_summary_line(line);
}

static unsigned int find_section(char **lines, const char *name,
				 bool maybe_one_liner)
{
	unsigned int i;

	for (i = 0; lines[i]; i++) {
		if (!is_section(lines[i], maybe_one_liner))
			continue;
		if (strncasecmp(lines[i], name, strlen(name)) != 0)
			continue;
		if (lines[i][strlen(name)] == ':')
			break;
	}
	return i;
}

/* function is NULL if we don't care. */
static unsigned int find_summary(char **lines, const char *function)
{
	unsigned int i;

	for (i = 0; lines[i]; i++) {
		if (!is_summary_line(lines[i]))
			continue;
		if (function) {
			if (!strstarts(lines[i], function))
				continue;
			if (!strstarts(lines[i] + strlen(function), " - "))
				continue;
		}
		break;
	}
	return i;
}


int main(int argc, char *argv[])
{
	unsigned int i;
	const char *type;
	const char *function = NULL;

	if (argc < 3)
		errx(1, "Usage: doc_extract [--function=<funcname>] TYPE <file>...\n"
		     "Where TYPE is functions|author|licence|maintainer|summary|description|example|all");

	if (strstarts(argv[1], "--function=")) {
		function = argv[1] + strlen("--function=");
		argv++;
		argc--;
	}

	type = argv[1];
	for (i = 2; i < argc; i++) {
		unsigned int line;
		char **lines = grab_doc(argv[i]);

		if (!lines[0])
			errx(1, "No documentation in file %s", argv[i]);

		if (function) {
			/* Allow us to trawl multiple files for a function */
			line = find_summary(lines, function);
			if (!lines[line])
				continue;

			/* Trim to just this function then. */
			lines += line;
			lines[find_summary(lines+1, NULL)] = NULL;
		}
		/* Simple one-line fields. */
		if (streq(type, "author")
		    || streq(type, "maintainer")
		    || streq(type, "licence")) {
			line = find_section(lines, type, true);
			if (lines[line]) {
				const char *p = strchr(lines[line], ':') + 1;
				p += strspn(p, " \t\n");
				if (p[0] == '\0') {
					/* Must be on next line. */
					if (end_section(lines[line+1]))
						errx(1, "Malformed %s", type);
					puts(lines[line+1]);
				} else
					puts(p);
			}
		} else if (streq(type, "summary")) {
			/* Summary comes after - on first line. */
			char *dash;

			dash = strchr(lines[0], '-');
			if (!dash)
				errx(1, "Malformed first line: no -");
			dash += strspn(dash, "- ");
			puts(dash);
		} else if (streq(type, "description")) {
			line = 1;
			while (is_blank(lines[line]))
				line++;

			while (!end_section(lines[line]))
				puts(lines[line++]);
		} else if (streq(type, "example")) {
			line = find_section(lines, type, false);
			if (lines[line]) {
				unsigned int strip;
				line++;

				while (is_blank(lines[line]))
					line++;

				/* Examples can be indented.  Take cue
				 * from first non-blank line. */
				if (lines[line])
					strip = strspn(lines[line], " \t");

				while (!end_section(lines[line])) {
					if (strspn(lines[line], " \t") >= strip)
						puts(lines[line] + strip);
					else
						puts(lines[line]);
					line++;
				}
			}
		} else if (streq(type, "functions")) {
			while (lines[line = find_summary(lines, NULL)]) {
				const char *dash = strstr(lines[line], " - ");
				printf("%.*s\n",
				       dash - lines[line], lines[line]);
				lines += line+1;
			}
		} else if (streq(type, "all")) {
			for (line = 0; lines[line]; line++)
				puts(lines[line]);
		} else
			errx(1, "Unknown type '%s'", type);
	}
	return 0;
}
