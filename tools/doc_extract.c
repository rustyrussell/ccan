/* This merely extracts, doesn't do XML or anything. */
#include <err.h>
#include <string.h>
#include <stdio.h>
#include <ccan/str/str.h>
#include <ccan/str_talloc/str_talloc.h>
#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include "doc_extract.h"

int main(int argc, char *argv[])
{
	unsigned int i;
	const char *type;
	const char *function = NULL;

	if (argc < 3)
		errx(1, "Usage: doc_extract [--function=<funcname>] TYPE <file>...\n"
		     "Where TYPE is functions|author|license|maintainer|summary|description|example|all");

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

		file = grab_file(NULL, argv[i], NULL);
		if (!file)
			err(1, "Reading file %s", argv[i]);
		lines = strsplit(file, file, "\n");

		list = extract_doc_sections(lines);
		if (list_empty(list))
			errx(1, "No documentation in file %s", argv[i]);
		talloc_free(file);

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
				else if (!streq(d->type, type))
					continue;

				for (j = 0; j < d->num_lines; j++)
					printf("%s\n", d->lines[j]);
			}
		}
		talloc_free(list);
	}
	return 0;
}
