#include <ccan/failtest/failtest.c>
#include <stdlib.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <ccan/tap/tap.h>

int main(void)
{
	int fds[2], fd;
	void *p;

	plan_tests(14);
	failtest_init(0, NULL);

	failpath = "mceopwrMCEOPWR";

	ok1((p = failtest_malloc(10, "run-failpath.c", 1)) != NULL);
	ok1(failtest_calloc(10, 5, "run-failpath.c", 1) != NULL);
	ok1((p = failtest_realloc(p, 100, "run-failpath.c", 1)) != NULL);
	ok1((fd = failtest_open("failpath-scratch", "run-failpath.c", 1,
				O_RDWR|O_CREAT, 0600)) >= 0);
	ok1(failtest_pipe(fds, "run-failpath.c", 1) == 0);
	ok1(failtest_write(fd, "xxxx", 4, "run-failpath.c", 1) == 4);
	lseek(fd, 0, SEEK_SET);
	ok1(failtest_read(fd, p, 5, "run-failpath.c", 1) == 4);

	/* Now we're into the failures. */
	ok1(failtest_malloc(10, "run-failpath.c", 1) == NULL);
	ok1(failtest_calloc(10, 5, "run-failpath.c", 1) == NULL);
	ok1(failtest_realloc(p, 100, "run-failpath.c", 1) == NULL);
	ok1(failtest_open("failpath-scratch", "run-failpath.c", 1,
			  O_RDWR|O_CREAT, 0600) == -1);
	ok1(failtest_pipe(fds, "run-failpath.c", 1) == -1);
	ok1(failtest_write(fd, "xxxx", 4, "run-failpath.c", 1) == -1);
	lseek(fd, 0, SEEK_SET);
	ok1(failtest_read(fd, p, 5, "run-failpath.c", 1) == -1);
	return exit_status();
}
