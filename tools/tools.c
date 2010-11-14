#include <ccan/talloc/talloc.h>
#include <ccan/grab_file/grab_file.h>
#include <ccan/noerr/noerr.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/noerr/noerr.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>
#include <unistd.h>
#include <assert.h>
#include "tools.h"

static char *tmpdir = NULL;
bool tools_verbose = false;

/* Ten minutes. */
const unsigned int default_timeout_ms = 10 * 60 * 1000;

char *talloc_basename(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return talloc_strdup(ctx, dir);
	return talloc_strdup(ctx, p+1);
}

char *talloc_dirname(const void *ctx, const char *dir)
{
	char *p = strrchr(dir, '/');

	if (!p)
		return talloc_strdup(ctx, ".");
	return talloc_strndup(ctx, dir, p - dir);
}

char *talloc_getcwd(const void *ctx)
{
	unsigned int len;
	char *cwd;

	/* *This* is why people hate C. */
	len = 32;
	cwd = talloc_array(ctx, char, len);
	while (!getcwd(cwd, len)) {
		if (errno != ERANGE) {
			talloc_free(cwd);
			return NULL;
		}
		cwd = talloc_realloc(ctx, cwd, char, len *= 2);
	}
	return cwd;
}

static void killme(int sig)
{
	kill(-getpid(), SIGKILL);
}

char *run_with_timeout(const void *ctx, const char *cmd,
		       bool *ok, unsigned *timeout_ms)
{
	pid_t pid;
	int p[2];
	char *ret;
	int status, ms;
	struct timeval start, end;

	*ok = false;
	if (pipe(p) != 0)
		return talloc_asprintf(ctx, "Failed to create pipe: %s",
				       strerror(errno));

	if (tools_verbose)
		printf("Running: %s\n", cmd);

	gettimeofday(&start, NULL);
	pid = fork();
	if (pid == -1) {
		close_noerr(p[0]);
		close_noerr(p[1]);
		return talloc_asprintf(ctx, "Failed to fork: %s",
				       strerror(errno));
		return NULL;
	}

	if (pid == 0) {
		struct itimerval itim;

		if (dup2(p[1], STDOUT_FILENO) != STDOUT_FILENO
		    || dup2(p[1], STDERR_FILENO) != STDERR_FILENO
		    || close(p[0]) != 0
		    || close(STDIN_FILENO) != 0
		    || open("/dev/null", O_RDONLY) != STDIN_FILENO)
			exit(128);

		signal(SIGALRM, killme);
		itim.it_interval.tv_sec = itim.it_interval.tv_usec = 0;
		itim.it_value.tv_sec = *timeout_ms / 1000;
		itim.it_value.tv_usec = (*timeout_ms % 1000) * 1000;
		setitimer(ITIMER_REAL, &itim, NULL);

		status = system(cmd);
		if (WIFEXITED(status))
			exit(WEXITSTATUS(status));
		/* Here's a hint... */
		exit(128 + WTERMSIG(status));
	}

	close(p[1]);
	ret = grab_fd(ctx, p[0], NULL);
	/* This shouldn't fail... */
	if (waitpid(pid, &status, 0) != pid)
		err(1, "Failed to wait for child");

	gettimeofday(&end, NULL);
	if (end.tv_usec < start.tv_usec) {
		end.tv_usec += 1000000;
		end.tv_sec--;
	}
	ms = (end.tv_sec - start.tv_sec) * 1000
		+ (end.tv_usec - start.tv_usec) / 1000;
	if (ms > *timeout_ms)
		*timeout_ms = 0;
	else
		*timeout_ms -= ms;
	close(p[0]);
	if (tools_verbose) {
		printf("%s", ret);
		printf("Finished: %u ms, %s %u\n", ms,
		       WIFEXITED(status) ? "exit status" : "killed by signal",
		       WIFEXITED(status) ? WEXITSTATUS(status)
		       : WTERMSIG(status));
	}
	*ok = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return ret;
}

/* Tallocs *output off ctx; return false if command fails. */
bool run_command(const void *ctx, unsigned int *time_ms, char **output,
		 const char *fmt, ...)
{
	va_list ap;
	char *cmd;
	bool ok;
	unsigned int default_time = default_timeout_ms;

	if (!time_ms)
		time_ms = &default_time;
	else if (*time_ms == 0) {
		*output = talloc_strdup(ctx, "\n== TIMED OUT ==\n");
		return false;
	}

	va_start(ap, fmt);
	cmd = talloc_vasprintf(ctx, fmt, ap);
	va_end(ap);

	*output = run_with_timeout(ctx, cmd, &ok, time_ms);
	if (ok)
		return true;
	if (!*output)
		err(1, "Problem running child");
	if (*time_ms == 0)
		*output = talloc_asprintf_append(*output,
						 "\n== TIMED OUT ==\n");
	return false;
}

static int unlink_all(char *dir)
{
	char cmd[strlen(dir) + sizeof("rm -rf ")];
	sprintf(cmd, "rm -rf %s", dir);
	if (tools_verbose)
		printf("Running: %s\n", cmd);
	if (system(cmd) != 0)
		warn("Could not remove temporary work in %s", dir);
	return 0;
}

char *temp_dir(const void *ctx)
{
	/* For first call, create dir. */
	while (!tmpdir) {
		tmpdir = getenv("TMPDIR");
		if (!tmpdir)
			tmpdir = "/tmp";
		tmpdir = talloc_asprintf(talloc_autofree_context(),
					 "%s/ccanlint-%u.%lu",
					 tmpdir, getpid(), random());
		if (mkdir(tmpdir, 0700) != 0) {
			if (errno == EEXIST) {
				talloc_free(tmpdir);
				tmpdir = NULL;
				continue;
			}
			err(1, "mkdir %s failed", tmpdir);
		}
		talloc_set_destructor(tmpdir, unlink_all);
		if (tools_verbose)
			printf("Created temporary directory %s\n", tmpdir);
	}
	return tmpdir;
}

char *maybe_temp_file(const void *ctx, const char *extension, bool keep,
		      const char *srcname)
{
	unsigned baselen;
	char *f, *suffix = talloc_strdup(ctx, "");
	struct stat st;
	unsigned int count = 0;

	if (!keep)
		srcname = talloc_basename(ctx, srcname);
	else
		assert(srcname[0] == '/');

	if (strrchr(srcname, '.'))
		baselen = strrchr(srcname, '.') - srcname;
	else
		baselen = strlen(srcname);

	do {
		f = talloc_asprintf(ctx, "%s/%.*s%s%s",
				    keep ? "" : temp_dir(ctx),
				    baselen, srcname,
				    suffix, extension);
		talloc_free(suffix);
		suffix = talloc_asprintf(ctx, "-%u", ++count);
	} while (lstat(f, &st) == 0);

	if (tools_verbose)
		printf("Creating file %s\n", f);

	talloc_free(suffix);
	return f;
}

bool move_file(const char *oldname, const char *newname)
{
	char *contents;
	size_t size;
	int fd;
	bool ret;

	if (tools_verbose)
		printf("Moving file %s to %s: ", oldname, newname);

	/* Simple case: rename works. */
	if (rename(oldname, newname) == 0) {
		if (tools_verbose)
			printf("rename worked\n");
		return true;
	}

	/* Try copy and delete: not atomic! */
	contents = grab_file(NULL, oldname, &size);
	if (!contents) {
		if (tools_verbose)
			printf("read failed: %s\n", strerror(errno));
		return false;
	}

	fd = open(newname, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (fd < 0) {
		if (tools_verbose)
			printf("output open failed: %s\n", strerror(errno));
		ret = false;
		goto free;
	}

	ret = write_all(fd, contents, size);
	if (close(fd) != 0)
		ret = false;

	if (ret) {
		if (tools_verbose)
			printf("copy worked\n");
		unlink(oldname);
	} else {
		if (tools_verbose)
			printf("write/close failed\n");
		unlink(newname);
	}

free:
	talloc_free(contents);
	return ret;
}
