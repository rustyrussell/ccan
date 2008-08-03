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


int main(int argc, char *argv[])
{
	unsigned int i, j;

	for (i = 1; i < argc; i++) {
		char *file;
		char **lines;
		bool printing = false, printed = false;

		file = grab_file(NULL, argv[i]);
		if (!file)
			err(1, "Reading file %s", argv[i]);
		lines = strsplit(file, file, "\n", NULL);

		for (j = 0; lines[j]; j++) {
			if (streq(lines[j], "/**")) {
				printing = true;
				if (printed++)
					puts("\n");
			} else if (streq(lines[j], " */"))
				printing = false;
			else if (printing) {
				if (strstarts(lines[j], " * "))
					puts(lines[j] + 3);
				else if (strstarts(lines[j], " *"))
					puts(lines[j] + 2);
				else
					errx(1, "Malformed line %s:%u",
					     argv[i], j);
			}
		}
		talloc_free(file);
	}
	return 0;
}

		
		
