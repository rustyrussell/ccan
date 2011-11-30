/* Include the C files directly. */
#include <ccan/failtest/failtest.c>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <ccan/tap/tap.h>

int main(int argc, char *argv[])
{
	int fd;
	char *p;
	char buf[] = "Hello world!";

	plan_tests(5);
	failtest_init(argc, argv);

	fd = failtest_open("run-write-scratchpad", __FILE__, __LINE__,
			   O_RDWR|O_CREAT, 0600);
	/* Child will fail, ignore. */
	if (fd < 0)
		failtest_exit(0);
	if (write(fd, buf, strlen(buf)) != strlen(buf))
		abort();
	ok1(lseek(fd, 0, SEEK_CUR) == strlen(buf));

	p = failtest_malloc(100, __FILE__, __LINE__);
	if (!p) {
		/* We are the child.  Do a heap of writes. */
		unsigned int i;

		for (i = 0; i < strlen(buf)+1; i++)
			if (failtest_write(fd, "x", 1, __FILE__, __LINE__)
			    == 1)
				break;
		failtest_close(fd, __FILE__, __LINE__);
		failtest_exit(0);
	}

	/* Seek pointer should be left alone! */
	ok1(lseek(fd, 0, SEEK_CUR) == strlen(buf));
	/* Length should be restored. */
	ok1(lseek(fd, 0, SEEK_END) == strlen(buf));
	lseek(fd, 0, SEEK_SET);
	ok1(read(fd, buf, strlen(buf)) == strlen("Hello world!"));
	ok1(strcmp(buf, "Hello world!") == 0);
	failtest_close(fd, __FILE__, __LINE__);

	return exit_status();
}
