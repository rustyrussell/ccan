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

/* This version adds one byte (for nul term) */
static void *grab_file(void *ctx, const char *filename)
{
	unsigned int max = 16384, size = 0;
	int ret, fd;
	char *buffer;

	if (streq(filename, "-"))
		fd = dup(STDIN_FILENO);
	else
		fd = open(filename, O_RDONLY, 0);

	if (fd < 0)
		return NULL;

	buffer = talloc_array(ctx, char, max+1);
	while ((ret = read(fd, buffer + size, max - size)) > 0) {
		size += ret;
		if (size == max)
			buffer = talloc_realloc(ctx, buffer, char, max*=2 + 1);
	}
	if (ret < 0) {
		talloc_free(buffer);
		buffer = NULL;
	} else
		buffer[size] = '\0';
	close(fd);
	return buffer;
}

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

		
		
