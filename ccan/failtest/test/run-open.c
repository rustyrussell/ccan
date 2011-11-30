/* Include the C files directly. */
#include <ccan/failtest/failtest.c>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(void)
{
	int fd, pfd[2], err;
	char buf[] = "Hello world!";
	struct stat st;

	plan_tests(12);
	failtest_init(0, NULL);

	if (pipe(pfd))
		abort();
	fd = failtest_open("run-open-scratchpad", "run-open.c", 1,
			   O_RDWR|O_CREAT, 0600);
	if (fd == -1) {
		/* We are the child: write error code for parent to check. */
		err = errno;
		if (write(pfd[1], &err, sizeof(err)) != sizeof(err))
			abort();
		failtest_exit(0);
	}
	/* Check it is read-write. */
	ok1(write(fd, buf, strlen(buf)) == strlen(buf));
	lseek(fd, SEEK_SET, 0);
	ok1(read(fd, buf, strlen("Hello world!")) == strlen("Hello world!"));
	ok1(strcmp(buf, "Hello world!") == 0);

	/* Check name and perms. */
	ok1(stat("run-open-scratchpad", &st) == 0);
	ok1(st.st_size == strlen(buf));
	ok1(S_ISREG(st.st_mode));
	ok1((st.st_mode & 0777) == 0600);

	/* Check child got correct errno. */
	ok1(read(pfd[0], &err, sizeof(err)) == sizeof(err));
	ok1(err == EACCES);

	/* Clean up. */
	failtest_close(fd, "run-open.c", 1);
	close(pfd[0]);
	close(pfd[1]);

	/* Two-arg open. */
	if (pipe(pfd) != 0)
		abort();
	fd = failtest_open("run-open-scratchpad", "run-open.c", 1, O_RDONLY);
	if (fd == -1) {
		/* We are the child: write error code for parent to check. */
		err = errno;
		if (write(pfd[1], &err, sizeof(err)) != sizeof(err))
			abort();
		failtest_exit(0);
	}
	/* Check it is read-only. */
	ok1(write(fd, buf, strlen(buf)) == -1);
	ok1(read(fd, buf, strlen("Hello world!")) == strlen("Hello world!"));
	ok1(strcmp(buf, "Hello world!") == 0);
	/* Clean up. */
	failtest_close(fd, "run-open.c", 1);
	close(pfd[0]);
	close(pfd[1]);

	return exit_status();
}
