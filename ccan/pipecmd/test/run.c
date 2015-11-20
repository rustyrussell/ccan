#include <ccan/pipecmd/pipecmd.h>
/* Include the C files directly. */
#include <ccan/pipecmd/pipecmd.c>
#include <ccan/tap/tap.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	pid_t child;
	int infd, outfd, status;
	char buf[5] = "test";

	/* We call ourselves, to test pipe. */
	if (argc == 2) {
		if (strcmp(argv[1], "out") == 0) {
			if (write(STDOUT_FILENO, buf, sizeof(buf)) != sizeof(buf))
				exit(1);
		} else if (strcmp(argv[1], "in") == 0) {
			if (read(STDIN_FILENO, buf, sizeof(buf)) != sizeof(buf))
				exit(1);
			if (memcmp(buf, "test", sizeof(buf)) != 0)
				exit(1);
		} else if (strcmp(argv[1], "inout") == 0) {
			if (read(STDIN_FILENO, buf, sizeof(buf)) != sizeof(buf))
				exit(1);
			buf[0]++;
			if (write(STDOUT_FILENO, buf, sizeof(buf)) != sizeof(buf))
				exit(1);
		} else
			abort();
		exit(0);
	}

	/* This is how many tests you plan to run */
	plan_tests(28);
	child = pipecmd(&outfd, &infd, argv[0], "inout", NULL);
	if (!ok1(child > 0))
		exit(1);
	ok1(write(infd, buf, sizeof(buf)) == sizeof(buf));
	ok1(read(outfd, buf, sizeof(buf)) == sizeof(buf));
	buf[0]--;
	ok1(memcmp(buf, "test", sizeof(buf)) == 0);
	ok1(waitpid(child, &status, 0) == child);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	child = pipecmd(NULL, &infd, argv[0], "in", NULL);
	if (!ok1(child > 0))
		exit(1);
	ok1(write(infd, buf, sizeof(buf)) == sizeof(buf));
	ok1(waitpid(child, &status, 0) == child);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	child = pipecmd(&outfd, NULL, argv[0], "out", NULL);
	if (!ok1(child > 0))
		exit(1);
	ok1(read(outfd, buf, sizeof(buf)) == sizeof(buf));
	ok1(memcmp(buf, "test", sizeof(buf)) == 0);
	ok1(waitpid(child, &status, 0) == child);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	// Writing to /dev/null should be fine.
	child = pipecmd(NULL, NULL, argv[0], "out", NULL);
	if (!ok1(child > 0))
		exit(1);
	ok1(waitpid(child, &status, 0) == child);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 0);

	// Reading should fail.
	child = pipecmd(NULL, NULL, argv[0], "in", NULL);
	if (!ok1(child > 0))
		exit(1);
	ok1(waitpid(child, &status, 0) == child);
	ok1(WIFEXITED(status));
	ok1(WEXITSTATUS(status) == 1);

	// Can't run non-existent file, but errno set correctly.
	child = pipecmd(NULL, NULL, "/doesnotexist", "in", NULL);
	ok1(errno == ENOENT);
	ok1(child < 0);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
