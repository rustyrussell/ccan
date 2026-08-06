/* Regression test for audit finding F5 (2026-08-05): failtest cannot
 * undo a child's write to an O_WRONLY file.
 *
 * failtest.c:save_contents() (line 410) saves the old contents with
 * pread(fd, ...), which fails with EBADF on an O_WRONLY fd; it warns
 * and saves s->count = 0.  restore_contents() then writes back 0 bytes
 * and ftruncates to old_len (failtest.c:470-478), so bytes the child
 * overwrote inside old_len are left modified: the parent's copy of the
 * file is corrupted and every later failtest child sees the corrupted
 * contents.  (The O_TRUNC open of a non-readable file has the same
 * problem via save_file(), failtest.c:1035.)
 *
 * Currently fails: test 1 (file contains "XYZlo world!").
 *
 * Temporary auditor-added test; no production files modified.
 */
#include <ccan/failtest/failtest.c>
#include <stdio.h>
#include <string.h>
#include <ccan/tap/tap.h>

#define FILE "run-wronly-write-scratch"

int main(int argc, char *argv[])
{
	int fd;
	void *p;
	char buf[16];

	alarm(10);

	/* Create the file with known contents (plain IO). */
	fd = open(FILE, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd < 0 || write(fd, "Hello world!", 12) != 12 || close(fd) != 0)
		abort();

	plan_tests(1);
	failtest_init(argc, argv);

	fd = failtest_open(FILE, __FILE__, __LINE__, O_WRONLY);
	if (fd < 0)
		failtest_exit(0);	/* open-failure child */

	p = failtest_malloc(10, __FILE__, __LINE__);
	if (!p) {
		/* malloc-failure child: really overwrite the first bytes. */
		if (failtest_write(fd, "XYZ", 3, __FILE__, __LINE__) != 3) {
			/* write-failure grandchild */
			failtest_close(fd, __FILE__, __LINE__);
			failtest_exit(0);
		}
		failtest_close(fd, __FILE__, __LINE__);
		failtest_exit(0);
	}
	failtest_free(p);
	failtest_close(fd, __FILE__, __LINE__);

	/* Parent: the child wrote "XYZ" over "Hel"; failtest should have
	 * restored the file. */
	fd = open(FILE, O_RDONLY);
	if (fd < 0)
		abort();
	if (read(fd, buf, 12) != 12)
		abort();
	close(fd);
	buf[12] = '\0';
	ok1(strcmp(buf, "Hello world!") == 0);
	if (strcmp(buf, "Hello world!") != 0)
		diag("file contents after child write: \"%s\" (expected \"Hello world!\")",
		     buf);

	failtest_exit(exit_status());
}
