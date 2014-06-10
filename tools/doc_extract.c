/* This merely extracts, doesn't do XML or anything. */
#include <ccan/str/str.h>
#include <ccan/err/err.h>
#include <ccan/tal/grab_file/grab_file.h>
#include "tools.h"
#include <string.h>
#include <stdio.h>
#include "doc_extract.h"

/* We regard non-alphanumerics as equiv. */
static bool typematch(const char *a, const char *b)
{
	size_t i;

	for (i = 0; a[i]; i++) {
		if (cisalnum(a[i])) {
			if (a[i] != b[i])
				return false;
		} else {
			if (cisalnum(b[i]))
				return false;
		}
	}
	return b[i] == '\0';
}

int main(int argc, char *argv[])
{
	unsigned int i;
	const char *type;
	const char *function = NULL;

	if (argc < 3)
		errx(1, "Usage: doc_extract [--function=<funcname>] TYPE <file>...\n"
		     "Where TYPE is functions|author|license|maintainer|summary|description|example|see_also|all");

	if (strstarts(argv[1], "--function=")) {
		function = argv[1] + strlen("--function=");
		argv++;
		argc--;
	}

	type = argv[1];
	for (i = 2; i < argc; i++) {
		char *file, **lines;
		struct list_head *list;
		struct doc_section *d;

		file = grab_file(NULL, argv[i]);
		if (!file)
			err(1, "Reading file %s", argv[i]);
		lines = tal_strsplit(file, file, "\n", STR_EMPTY_OK);

		list = extract_doc_sections(lines, argv[i]);
		if (list_empty(list))
			errx(1, "No documentation in file %s", argv[i]);
		tal_free(file);

		if (streq(type, "functions")) {
			const char *last = NULL;
			list_for_each(list, d, list) {
				if (d->function) {
					if (!last || !streq(d->function, last))
						printf("%s\n", d->function);
					last = d->function;
				}
			}
		} else {
			unsigned int j;
			list_for_each(list, d, list) {
				if (function) {
					if (!d->function)
						continue;
					if (!streq(d->function, function))
						continue;
				}
				if (streq(type, "all"))
					printf("%s:\n", d->type);
				else if (!typematch(d->type, type))
					continue;

				for (j = 0; j < d->num_lines; j++)
					printf("%s\n", d->lines[j]);
			}
		}
		tal_free(list);
	}
	return 0;
}
