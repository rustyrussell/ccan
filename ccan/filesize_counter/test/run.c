#include <ccan/filesize_counter/filesize_counter.h>
/* Include the C files directly. */
#include <ccan/filesize_counter/szcnt.c>
#include <ccan/tap/tap.h>
#include <err.h>
#include <stdint.h>

int main(void)
{
	int i;
	szcnt *h;
	struct stat s;

	/* This is how many tests you plan to run */
	plan_tests(35);

	ok1(h = szcnt_new());
	if (!h) exit(1);

	ok1(!h->nm && !h->tmp && h->fd == -1 && !h->cnt);

	if (unlink("t") == -1 && errno != ENOENT) err(1, "unlink");
	if (unlink(".t") == -1 && errno != ENOENT) err(1, "unlink");

	ok1(szcnt_open(h, "t") == 0);
	ok1(h->cnt == 0 && strcmp(h->nm, "t") == 0 && strcmp(h->tmp, ".t") == 0);
	int save = h->fd;

	if (stat("t", &s) == -1) err(1, "stat");
	ok1(s.st_size == 0 && s.st_blocks == 0);

	ok1(szcnt_inc(h) == 0);

	if (stat("t", &s) == -1) err(1, "stat");
	ok1(s.st_size == 1 && s.st_blocks == 8);

	for (i = 0; i < 4095 && h->fd == save; i++)
		szcnt_inc(h);

	ok1(h->fd == save);
	ok1(szcnt_inc(h) == 0);
	ok1(h->fd != save);

	if (stat("t", &s) == -1) err(1, "stat");
	ok1(s.st_size == 4097 && s.st_blocks == 8);

	for (i = 0; i < 4096; i++)
		szcnt_inc(h);

	if (stat("t", &s) == -1) err(1, "stat");
	ok1(s.st_size == 8193 && s.st_blocks == 8);

	ok1(szcnt_close(h) == 0);
	ok1(!h->nm && !h->tmp && h->fd == -1 && !h->cnt);
	ok1(szcnt_open(h, "t") == 8193 && h->cnt == 8193);

	ok1(szcnt_zero(h) == 0 && h->cnt == 0);
	if (stat("t", &s) == -1) err(1, "stat");
	ok1(s.st_size == 0 && s.st_blocks == 0);

	if (unlink("foo/bar/baz") == -1 && errno != ENOENT) err(1, "unlink");
	if (unlink("foo/bar/.baz") == -1 && errno != ENOENT) err(1, "unlink");
	if (rmdir("foo/bar/.oof") == -1 && errno != ENOENT) err(1, "rmdir");
	if (rmdir("foo/bar") == -1 && errno != ENOENT) err(1, "rmdir");
	if (rmdir("foo") == -1 && errno != ENOENT) err(1, "rmdir");
	if (mkdir("foo", 0777) == -1) err(1, "mkdir");
	if (mkdir("foo/bar", 0777) == -1) err(1, "mkdir");
	if (mkdir("foo/bar/.oof", 0777) == -1) err(1, "mkdir");

	ok1(szcnt_open(h, "foo/bar/baz") == 0);
	ok1(h->cnt == 0 && strcmp(h->nm, "foo/bar/baz") == 0 && strcmp(h->tmp, "foo/bar/.baz") == 0);
	ok1(szcnt_close(h) == 0);

	ok1(szcnt_open(h, "foo") == -1 && errno == EINVAL);
	ok1(!h->nm && !h->tmp && h->fd == -1 && !h->cnt);

	ok1(szcnt_open(h, "foo/bar/oof") == -1 && errno == EINVAL);
	ok1(!h->nm && !h->tmp && h->fd == -1 && !h->cnt);

	if (chmod("t", 0) == -1) err(1, "chmod");
	ok1(szcnt_open(h, "t") == -1 && errno == EACCES);
	ok1(!h->nm && !h->tmp && h->fd == -1 && !h->cnt);

	if (chmod("t", 0777) == -1) err(1, "chmod");
	ok1(szcnt_open(h, "t") == 0);

	int fake;
	if ((fake = open("t", O_RDONLY)) == -1) err(1, "open");
	save = h->fd;
	h->fd = fake;

	ok1(szcnt_inc(h) == -1 && errno == EBADF);
	ok1(szcnt_zero(h) == -1 && errno == EINVAL);
	char *save_tmp = h->tmp;
	h->tmp = (char *) ".";
	h->cnt = 4096;
	ok1(szcnt_inc(h) == -1 && errno == EISDIR);
	h->tmp = save_tmp;
	h->cnt = sizeof(off_t) == sizeof(int64_t) ? INT64_MAX : INT32_MAX;
	ok1(szcnt_inc(h) == -1 && errno == EINVAL);
	h->cnt = 0;

	if (unlink(".t") == -1 && errno != ENOENT) err(1, "unlink");
	if (unlink(".u") == -1 && errno != ENOENT) err(1, "unlink");
	if (symlink(".t", ".u") == -1) err(1, "symlink");
	if (symlink(".u", ".t") == -1) err(1, "symlink");
	ok1(szcnt_swap(h) == -1 && errno == ELOOP);

	close(fake);
	ok1(szcnt_close(h) == -1 && errno == EBADF);

	h->fd = save;
	ok1(szcnt_free(h) == 0);

	h = szcnt_new();
	ok1(szcnt_inc(h) == -1 && errno == EBADF);
	szcnt_close(h);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
