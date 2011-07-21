/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "grab_file.h"
#include <ccan/talloc/talloc.h>
#include <ccan/noerr/noerr.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void *grab_fd(const void *ctx, int fd, size_t *size)
{
	int ret;
	size_t max, s;
	char *buffer;
	struct stat st;

	if (!size)
		size = &s;
	*size = 0;

	if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode))
		max = st.st_size;
	else
		max = 16384;

	buffer = talloc_array(ctx, char, max+1);
	while ((ret = read(fd, buffer + *size, max - *size)) > 0) {
		*size += ret;
		if (*size == max) {
			buffer = talloc_realloc(ctx, buffer, char, max*2+1);
			if (!buffer) {
				buffer = talloc_realloc(ctx, buffer, char,
							max + 1024*1024 + 1);
				if (!buffer)
					return NULL;
				max += 1024*1024;
			} else
				max *= 2;
		}
	}
	if (ret < 0) {
		talloc_free(buffer);
		buffer = NULL;
	} else
		buffer[*size] = '\0';

	return buffer;
}

void *grab_file(const void *ctx, const char *filename, size_t *size)
{
	int fd;
	char *buffer;

	if (!filename)
		fd = dup(STDIN_FILENO);
	else
		fd = open(filename, O_RDONLY, 0);

	if (fd < 0)
		return NULL;

	buffer = grab_fd(ctx, fd, size);
	close_noerr(fd);
	return buffer;
}
