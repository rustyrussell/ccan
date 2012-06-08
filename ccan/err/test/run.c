#include <ccan/err/err.h>
#include <ccan/tap/tap.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_MAX 1024

int main(void)
{
	int pfd[2];

	plan_tests(20);
	fflush(stdout);

	/* Test err() in child */
	pipe(pfd);
	if (fork()) {
		char buffer[BUFFER_MAX+1];
		unsigned int i;
		int status;

		/* We are parent. */
		close(pfd[1]);
		for (i = 0; i < BUFFER_MAX; i++) {
			if (read(pfd[0], buffer + i, 1) == 0) {
				buffer[i] = '\0';
				ok1(strstr(buffer, "running err:"));
				ok1(strstr(buffer, strerror(ENOENT)));
				ok1(buffer[i-1] == '\n');
				break;
			}
		}
		close(pfd[0]);
		ok1(wait(&status) != -1);
		ok1(WIFEXITED(status));
		ok1(WEXITSTATUS(status) == 17);
	} else {
		close(pfd[0]);
		dup2(pfd[1], STDERR_FILENO);
		errno = ENOENT;
		err(17, "running %s", "err");
		abort();
	}

	/* Test errx() in child */
	pipe(pfd);
	fflush(stdout);
	if (fork()) {
		char buffer[BUFFER_MAX+1];
		unsigned int i;
		int status;

		/* We are parent. */
		close(pfd[1]);
		for (i = 0; i < BUFFER_MAX; i++) {
			if (read(pfd[0], buffer + i, 1) == 0) {
				buffer[i] = '\0';
				ok1(strstr(buffer, "running errx\n"));
				break;
			}
		}
		close(pfd[0]);
		ok1(wait(&status) != -1);
		ok1(WIFEXITED(status));
		ok1(WEXITSTATUS(status) == 17);
	} else {
		close(pfd[0]);
		dup2(pfd[1], STDERR_FILENO);
		errx(17, "running %s", "errx");
		abort();
	}


	/* Test warn() in child */
	pipe(pfd);
	fflush(stdout);
	if (fork()) {
		char buffer[BUFFER_MAX+1];
		unsigned int i;
		int status;

		/* We are parent. */
		close(pfd[1]);
		for (i = 0; i < BUFFER_MAX; i++) {
			if (read(pfd[0], buffer + i, 1) == 0) {
				buffer[i] = '\0';
				ok1(strstr(buffer, "running warn:"));
				ok1(strstr(buffer, strerror(ENOENT)));
				ok1(buffer[i-1] == '\n');
				break;
			}
		}
		close(pfd[0]);
		ok1(wait(&status) != -1);
		ok1(WIFEXITED(status));
		ok1(WEXITSTATUS(status) == 17);
	} else {
		close(pfd[0]);
		dup2(pfd[1], STDERR_FILENO);
		errno = ENOENT;
		warn("running %s", "warn");
		exit(17);
	}

	/* Test warnx() in child */
	pipe(pfd);
	fflush(stdout);
	if (fork()) {
		char buffer[BUFFER_MAX+1];
		unsigned int i;
		int status;

		/* We are parent. */
		close(pfd[1]);
		for (i = 0; i < BUFFER_MAX; i++) {
			if (read(pfd[0], buffer + i, 1) == 0) {
				buffer[i] = '\0';
				ok1(strstr(buffer, "running warnx\n"));
				break;
			}
		}
		close(pfd[0]);
		ok1(wait(&status) != -1);
		ok1(WIFEXITED(status));
		ok1(WEXITSTATUS(status) == 17);
	} else {
		close(pfd[0]);
		dup2(pfd[1], STDERR_FILENO);
		warnx("running %s", "warnx");
		exit(17);
	}
	return exit_status();
}

