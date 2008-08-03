#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include "string.h"
#include "talloc/talloc.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

char **strsplit(const void *ctx, const char *string, const char *delims,
		 unsigned int *nump)
{
	char **lines = NULL;
	unsigned int max = 64, num = 0;

	lines = talloc_array(ctx, char *, max+1);

	while (*string != '\0') {
		unsigned int len = strcspn(string, delims);
		lines[num] = talloc_array(lines, char, len + 1);
		memcpy(lines[num], string, len);
		lines[num][len] = '\0';
		string += len;
		string += strspn(string, delims) ? 1 : 0;
		if (++num == max)
			lines = talloc_realloc(ctx, lines, char *, max*=2 + 1);
	}
	lines[num] = NULL;
	if (nump)
		*nump = num;
	return lines;
}

char *strjoin(const void *ctx, char *strings[], const char *delim)
{
	unsigned int i;
	char *ret = talloc_strdup(ctx, "");

	for (i = 0; strings[i]; i++) {
		ret = talloc_append_string(ret, strings[i]);
		ret = talloc_append_string(ret, delim);
	}
	return ret;
}

static int close_no_errno(int fd)
{
	int ret = 0, serrno = errno;
	if (close(fd) < 0)
		ret = errno;
	errno = serrno;
	return ret;
}

void *grab_fd(const void *ctx, int fd)
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

void *grab_file(const void *ctx, const char *filename)
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
	close_no_errno(fd);
	return buffer;
}
