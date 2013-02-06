#include <ccan/softsync/softsync.h>
/* Include the C files directly. */
#include <ccan/softsync/softsync.c>
#include <ccan/tap/tap.h>

#define TESTFILE "testfile"

int main(void)
{
	int fd;
	char buf[31];
	const char expect_buf[30] = "hellothere";
	struct softsync *s = malloc(sizeof(*s)); /* malloc helps valgrind */

	/* This is how many tests you plan to run */
	plan_tests(14);

	/* Make sure we're clean */
	unlink(TESTFILE);
	unlink(TESTFILE SOFTSYNC_EXT);

	fd = softsync_open(s, TESTFILE, O_RDWR|O_CREAT, 0600);

	ok1(fd >= 0);
	ok1(s->base_fd == fd);
	ok1(s->journal_fd >= 0);

	ok1(softsync_pwrite(s, "hello", 5, 0) == 5);
	ok1(softsync_pwrite(s, "there", 5, 5) == 5);
	ok1(softsync_ftruncate(s, 30) == 0);

	ok1(softsync_fdatasync(s) == 0);
	ok1(softsync_close(s) == 0);

	fd = open(TESTFILE, O_RDWR);
	ok1(fd >= 0);
	ok1(read(fd, buf, 31) == 30);
	ok1(memcmp(buf, expect_buf, sizeof(expect_buf)) == 0);

	/* Now corrupt it a bit. */
	lseek(fd, 0, SEEK_SET);
	ok1(write(fd, "bye", 4) == 4);
	close(fd);

	fd = softsync_open(s, TESTFILE, O_RDWR, 0600);
	ok1(softsync_pread(s, buf, 31, 0) == 30);
	ok1(memcmp(buf, expect_buf, sizeof(expect_buf)) == 0);

	softsync_close(s);
	free(s);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
