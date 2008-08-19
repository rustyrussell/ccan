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
#include "talloc/talloc.h"
#include "string/string.h"

static char **grab_doc(const char *fname)
{
	char *file;
	char **lines, **ret;
	unsigned int i, num;
	bool printing = false, printed = false;

	file = grab_file(NULL, fname, NULL);
	if (!file)
		err(1, "Reading file %s", fname);
	lines = strsplit(file, file, "\n", &num);
	ret = talloc_array(NULL, char *, num+1);

	num = 0;
	for (i = 0; lines[i]; i++) {
		if (streq(lines[i], "/**")) {
			printing = true;
			if (printed++)
				talloc_append_string(ret[num], "\n");
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

static bool is_section(const char *line)
{
	unsigned int len;

	len = strcspn(line, " \t\n:");
	if (len == 0)
		return false;

	return line[len] == ':' && is_blank(line+len+1);
}


static bool end_section(const char *line)
{
	return !line || is_section(line);
}

static unsigned int find_section(char **lines, const char *name)
{
	unsigned int i;

	for (i = 0; lines[i]; i++) {
		if (!is_section(lines[i]))
			continue;
		if (strncasecmp(lines[i], name, strlen(name)) != 0)
			continue;
		if (lines[i][strlen(name)] == ':')
			break;
	}
	return i;
}

int main(int argc, char *argv[])
{
	unsigned int i;
	const char *type;

	if (argc < 3)
		errx(1, "Usage: doc_extract TYPE <file>...\n"
		     "Where TYPE is author|licence|maintainer|summary|description|example|all");

	type = argv[1];
	for (i = 2; i < argc; i++) {
		unsigned int line;
		char **lines = grab_doc(argv[i]);

		if (!lines[0])
			errx(1, "No documentation in file");

		/* Simple one-line fields. */
		if (streq(type, "author")
		    || streq(type, "maintainer")
		    || streq(type, "licence")) {
			line = find_section(lines, type);
			if (lines[line]) {
				if (!lines[line+1])
					errx(1, "Malformed %s, end of file",
					     type);
				puts(lines[line+1]);
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
			line = find_section(lines, type);
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
		} else if (streq(type, "all")) {
			for (line = 0; lines[line]; line++)
				puts(lines[line]);
		} else
			errx(1, "Unknown type '%s'", type);
			
		talloc_free(lines);
	}
	return 0;
}

		
		
