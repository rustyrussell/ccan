/* Include the C files directly. */
#include <ccan/failtest/failtest.c>
#include <stdlib.h>
#include <err.h>
#include <ccan/tap/tap.h>

int main(void)
{
	int fd, pfd[2], ecode;
	struct rlimit lim;

	if (getrlimit(RLIMIT_NOFILE, &lim) != 0)
		err(1, "getrlimit RLIMIT_NOFILE fail?");

	printf("rlimit = %lu/%lu (inf=%lu)\n",
	       (long)lim.rlim_cur, (long)lim.rlim_max,
	       (long)RLIM_INFINITY);
	lim.rlim_cur /= 2;
	if (lim.rlim_cur < 8)
		errx(1, "getrlimit limit %li too low", (long)lim.rlim_cur);
	if (setrlimit(RLIMIT_NOFILE, &lim) != 0)
		err(1, "setrlimit RLIMIT_NOFILE (%li/%li)",
		    (long)lim.rlim_cur, (long)lim.rlim_max);

	plan_tests(2);
	failtest_init(0, NULL);

	if (pipe(pfd))
		abort();

	fd = failtest_open("run-with-fdlimit-scratch", "run-with_fdlimit.c", 1,
			   O_RDWR|O_CREAT, 0600);
	if (fd == -1) {
		/* We are the child: write error code for parent to check. */
		ecode = errno;
		if (write(pfd[1], &ecode, sizeof(ecode)) != sizeof(ecode))
			abort();
		failtest_exit(0);
	}

	/* Check child got correct errno. */
	ok1(read(pfd[0], &ecode, sizeof(ecode)) == sizeof(ecode));
	ok1(ecode == EACCES);

	/* Clean up. */
	failtest_close(fd, "run-open.c", 1);
	close(pfd[0]);
	close(pfd[1]);

	return exit_status();
}
