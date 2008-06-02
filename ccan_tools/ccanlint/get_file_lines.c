#include "get_file_lines.h"
#include <talloc/talloc.h>
#include <string/string.h>
#include <noerr/noerr.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <dirent.h>

static void *grab_fd(const void *ctx, int fd)
{
	int ret;
	unsigned int max = 16384, size = 0;
	char *buffer;

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

	return buffer;
}

/* This version adds one byte (for nul term) */
static void *grab_file(const void *ctx, const char *filename)
{
	int fd;
	char *buffer;

	if (streq(filename, "-"))
		fd = dup(STDIN_FILENO);
	else
		fd = open(filename, O_RDONLY, 0);

	if (fd < 0)
		return NULL;

	buffer = grab_fd(ctx, fd);
	close_noerr(fd);
	return buffer;
}

/* This is a dumb one which copies.  We could mangle instead. */
static char **split(const void *ctx, const char *text, const char *delims,
		    unsigned int *nump)
{
	char **lines = NULL;
	unsigned int max = 64, num = 0;

	lines = talloc_array(ctx, char *, max+1);

	while (*text != '\0') {
		unsigned int len = strcspn(text, delims);
		lines[num] = talloc_array(lines, char, len + 1);
		memcpy(lines[num], text, len);
		lines[num][len] = '\0';
		text += len;
		text += strspn(text, delims);
		if (++num == max)
			lines = talloc_realloc(ctx, lines, char *, max*=2 + 1);
	}
	lines[num] = NULL;
	if (nump)
		*nump = num;
	return lines;
}

char **get_file_lines(void *ctx, const char *name, unsigned int *num_lines)
{
	char *buffer = grab_file(ctx, name);

	if (!buffer)
		err(1, "Getting file %s", name);

	return split(buffer, buffer, "\n", num_lines);
}
