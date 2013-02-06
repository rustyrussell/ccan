#include <ccan/softsync/softsync.h>
/* Include the C files directly. */
#include <ccan/softsync/softsync.c>
#include <ccan/tap/tap.h>

#define TESTFILE "testfile-pread"

static bool check(struct softsync *s)
{
	int start, len;
	char *expect, *buf;

	if (!softsync_check(s, NULL))
		return false;

	expect = malloc(s->base_len);
	pread(s->base_fd, expect, s->base_len, 0);

	assert(!s->needs_recovery);
	s->needs_recovery = true;

	/* Check every pread offset. */
	for (start = 0; start < s->base_len; start++) {
		for (len = 1; start + len < s->base_len; start++) {
			buf = malloc(len);
			if (softsync_pread(s, buf, len, start) != len)
				return false;
			if (memcmp(buf, expect + start, len) != 0)
				return false;
			free(buf);
		}
	}
	free(expect);
	s->needs_recovery = false;
	return true;
}

int main(void)
{
	int fd;
	struct softsync *s = malloc(sizeof(*s)); /* malloc helps valgrind */

	/* This is how many tests you plan to run */
	plan_tests(15);

	/* Make sure we're clean */
	unlink(TESTFILE);
	unlink(TESTFILE SOFTSYNC_EXT);

	fd = softsync_open(s, TESTFILE, O_RDWR|O_CREAT, 0600);

	ok1(fd >= 0);
	ok1(s->base_fd == fd);
	ok1(s->journal_fd >= 0);

	/* Do several overlapping writes, and some truncates. */
	ok1(softsync_pwrite(s, "abcxy", 5, 0) == 5);
	ok1(check(s));
	ok1(softsync_pwrite(s, "xghij", 5, 5) == 5);
	ok1(check(s));
	ok1(softsync_pwrite(s, "def", 3, 3) == 3);
	ok1(check(s));
	ok1(softsync_ftruncate(s, 8) == 0);
	ok1(check(s));
	ok1(softsync_pwrite(s, "qrs", 3, 10) == 3);
	ok1(check(s));
	ok1(softsync_ftruncate(s, 30) == 0);
	ok1(check(s));
	softsync_close(s);
	free(s);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
