#include <sys/types.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#define _XOPEN_SOURCE 700
#include <unistd.h>

#include <ccan/tap/tap.h>

int fail_calloc = 1, fail_malloc = 1, fail_ftruncate = 1, fail_rename = 1, fail_close = 0, fail_lseek = 1;
#define calloc(x, y) (fail_calloc ? NULL : calloc(x, y))
#define malloc(x) (fail_malloc ? NULL : malloc(x))
#define ftruncate(x, y) (fail_ftruncate ? -1 : ftruncate(x, y))
#define rename(x, y) (fail_rename ? -1 : rename(x, y))
#define close(x) (fail_close ? -1 : close(x))
#define lseek(x, y, z) (fail_lseek ? -1 : lseek(x, y, z))

#include <ccan/filesize_counter/filesize_counter.h>
#include <ccan/filesize_counter/szcnt.c>

int main(int argc, char *argv[])
{
	struct szcnt *h;

	plan_no_plan();

	ok1(szcnt_new() == NULL);
	fail_calloc = 0;
	ok1(h = szcnt_new());

	if (unlink("t") == -1 && errno != ENOENT) err(1, "unlink");
	if (unlink(".t") == -1 && errno != ENOENT) err(1, "unlink");

	ok1(szcnt_open(h, "t") == -1);
	fail_malloc = 0;
	ok1(szcnt_open(h, "t") == -1);
	fail_lseek = 0;
	ok1(szcnt_open(h, "t") == 0);

	fail_close = 1;
	ok1(szcnt_close(h) == -1);

	h->cnt = 4096;
	ok1(szcnt_inc(h) == -1);
	fail_ftruncate = 0;
	ok1(szcnt_inc(h) == -1);
	fail_rename = 0;
	ok1(szcnt_inc(h) == -1);
	fail_close = 0;

	ok1(szcnt_inc(h) == 0);

	szcnt_free(h);
	return exit_status();
}
