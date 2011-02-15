#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ccan/tap/tap.h>
/* Include the C files directly. */
#include <ccan/failtest/failtest.c>

int main(void)
{
	int fd;
	char *p;
	char buf[] = "Hello world!";

	plan_tests(5);

	fd = failtest_open("run-write-scratchpad", "run-write.c", 1,
			   O_RDWR|O_CREAT, 0600);
	/* Child will fail, ignore. */
	if (fd < 0)
		failtest_exit(0);
	write(fd, buf, strlen(buf));
	ok1(lseek(fd, 0, SEEK_CUR) == strlen(buf));

	p = failtest_malloc(100, "run-write.c", 1);
	if (!p) {
		/* We are the child.  Do a heap of writes. */
		unsigned int i;

		for (i = 0; i < strlen(buf)+1; i++)
			if (failtest_write(fd, "x", 1, "run-write.c", 1) == 1)
				break;
		failtest_exit(0);
	}

	/* Seek pointer should be left alone! */
	ok1(lseek(fd, 0, SEEK_CUR) == strlen(buf));
	/* Length should be restored. */
	ok1(lseek(fd, 0, SEEK_END) == strlen(buf));
	lseek(fd, 0, SEEK_SET);
	ok1(read(fd, buf, strlen(buf)) == strlen("Hello world!"));
	ok1(strcmp(buf, "Hello world!") == 0);

	return exit_status();
}
