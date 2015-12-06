#include "filecnt.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#define _XOPEN_SOURCE 700
#include <unistd.h>

filecnt *filecnt_new(void)
{
	filecnt *h = calloc(1, sizeof(filecnt));
	if (h) h->fd = -1;
	return h;
}

int filecnt_free(filecnt *h)
{
	int ret;

	assert(h);
	ret = filecnt_close(h);
	free(h);
	return ret;
}

static int isreg(const char *path)
{
	struct stat s;
	int ret = stat(path, &s);

	if (ret == -1 && errno == ENOENT)
		return 0;
	if (ret == 0 && S_ISREG(s.st_mode))
		return 0;
	if (ret == 0)
		errno = EINVAL;
	return -1;
}

off_t filecnt_open(filecnt *h, const char *path)
{
	int len, i;

	assert(h && path);

	// store original filename and temp filename in one buffer
	// temp name is original with a dot preceding basename
	len = strlen(path) + 1;
	assert(len > 1);
	if ((h->nm = malloc(len * 2 + 1)) == NULL)
		goto err;
	h->tmp = h->nm + len;

	for (i = len; i > 0; i--)
		if (path[i] == '/') {
			i++;
			break;
		}
	//                             orig  null  dirname  dot  basename
	sprintf(h->nm, "%s%c%.*s%c%s", path, '\0', i, path, '.', path + i);

	if (isreg(h->nm) == -1)
		goto err;
	if (isreg(h->tmp) == -1)
		goto err;

	if ((h->fd = open(h->nm, O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR)) == -1)
		goto err;

	return filecnt_sync(h);

err:
	filecnt_close(h);
	return -1;
}

int filecnt_close(filecnt *h)
{
	assert(h);

	if (h->fd != -1 && close(h->fd) == -1)
		return -1;
	h->fd = -1;

	if (h->nm)
		free(h->nm);
	h->nm  = NULL;
	h->tmp = NULL;

	h->cnt = 0;

	return 0;
}

static int filecnt_swap(filecnt *h)
{
	int fd;

	assert(h);

	if ((fd = open(h->tmp, O_WRONLY|O_CREAT|O_APPEND|O_TRUNC, S_IRUSR|S_IWUSR)) == -1)
		return -1;

	if (ftruncate(fd, h->cnt) == -1)
		return -1;

	if (rename(h->tmp, h->nm) == -1)
		return -1;

	if (close(h->fd) == -1)
		return -1;

	h->fd = fd;

	return 0;
}

off_t filecnt_sync(filecnt *h)
{
	off_t ret;

	assert(h);

	if ((ret = lseek(h->fd, 0, SEEK_END)) == -1)
		return -1;

	return h->cnt = ret;
}

int filecnt_zero(filecnt *h)
{
	assert(h);

	if (ftruncate(h->fd, 0) == -1)
		return -1;

	return h->cnt = 0;
}

int filecnt_inc(filecnt *h) {

	assert(h);

	if (h->fd == -1) {
		errno = EBADF;
		return -1;
	}

	if (h->cnt + 1 < 0 && filecnt_zero(h) == -1)
		return -1;

	if (h->cnt > 0 && h->cnt % 4096 == 0 && filecnt_swap(h) == -1)
		return -1;

	if (write(h->fd, "", 1) == -1)
		return -1;
	else h->cnt++;

	return 0;
}
