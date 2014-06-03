#include <ccan/take/take.h>
#include <ccan/err/err.h>
#include <ccan/noerr/noerr.h>
#include <ccan/rbuf/rbuf.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/noerr/noerr.h>
#include <ccan/time/time.h>
#include <ccan/tal/path/path.h>
#include <ccan/tal/grab_file/grab_file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include "tools.h"

static const char *tmpdir = NULL;
bool tools_verbose = false;

/* Ten minutes. */
const unsigned int default_timeout_ms = 10 * 60 * 1000;

static void killme(int sig)
{
	kill(-getpid(), SIGKILL);
}

char *run_with_timeout(const void *ctx, const char *cmd,
		       bool *ok, unsigned *timeout_ms)
{
	pid_t pid;
	int p[2];
	struct rbuf in;
	int status, ms;
	struct timeabs start;

	*ok = false;
	if (pipe(p) != 0)
		return tal_fmt(ctx, "Failed to create pipe: %s",
			       strerror(errno));

	if (tools_verbose)
		printf("Running: %s\n", cmd);

	/* Always flush buffers before fork! */
	fflush(stdout);
	start = time_now();
	pid = fork();
	if (pid == -1) {
		close_noerr(p[0]);
		close_noerr(p[1]);
		return tal_fmt(ctx, "Failed to fork: %s", strerror(errno));
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
		itim.it_value = timespec_to_timeval(time_from_msec(*timeout_ms).ts);
		setitimer(ITIMER_REAL, &itim, NULL);

		status = system(cmd);
		if (WIFEXITED(status))
			exit(WEXITSTATUS(status));
		/* Here's a hint... */
		exit(128 + WTERMSIG(status));
	}

	close(p[1]);
	rbuf_init(&in, p[0], tal_arr(ctx, char, 4096), 4096);
	if (!rbuf_read_str(&in, 0, do_tal_realloc) && errno)
		in.buf = tal_free(in.buf);

	/* This shouldn't fail... */
	if (waitpid(pid, &status, 0) != pid)
		err(1, "Failed to wait for child");

	ms = time_to_msec(time_between(time_now(), start));
	if (ms > *timeout_ms)
		*timeout_ms = 0;
	else
		*timeout_ms -= ms;
	close(p[0]);
	if (tools_verbose) {
		printf("%s", in.buf);
		printf("Finished: %u ms, %s %u\n", ms,
		       WIFEXITED(status) ? "exit status" : "killed by signal",
		       WIFEXITED(status) ? WEXITSTATUS(status)
		       : WTERMSIG(status));
	}
	*ok = (WIFEXITED(status) && WEXITSTATUS(status) == 0);
	return in.buf;
}

/* Tals *output off ctx; return false if command fails. */
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
		*output = tal_strdup(ctx, "\n== TIMED OUT ==\n");
		return false;
	}

	va_start(ap, fmt);
	cmd = tal_vfmt(ctx, fmt, ap);
	va_end(ap);

	*output = run_with_timeout(ctx, cmd, &ok, time_ms);
	if (ok)
		return true;
	if (!*output)
		err(1, "Problem running child");
	if (*time_ms == 0)
		*output = tal_strcat(ctx, take(*output), "\n== TIMED OUT ==\n");
	return false;
}

static void unlink_all(const char *dir)
{
	char cmd[strlen(dir) + sizeof("rm -rf ")];
	sprintf(cmd, "rm -rf %s", dir);
	if (tools_verbose)
		printf("Running: %s\n", cmd);
	if (system(cmd) != 0)
		warn("Could not remove temporary work in %s", dir);
}

static pid_t *afree;
static void free_autofree(void)
{
	if (*afree == getpid())
		tal_free(afree);
}

tal_t *autofree(void)
{
	if (!afree) {
		afree = tal(NULL, pid_t);
		*afree = getpid();
		atexit(free_autofree);
	}
	return afree;
}

const char *temp_dir(void)
{
	/* For first call, create dir. */
	while (!tmpdir) {
		tmpdir = getenv("TMPDIR");
		if (!tmpdir)
			tmpdir = "/tmp";
		tmpdir = tal_fmt(autofree(), "%s/ccanlint-%u.%lu",
				 tmpdir, getpid(), random());
		if (mkdir(tmpdir, 0700) != 0) {
			if (errno == EEXIST) {
				tal_free(tmpdir);
				tmpdir = NULL;
				continue;
			}
			err(1, "mkdir %s failed", tmpdir);
		}
		tal_add_destructor(tmpdir, unlink_all);
		if (tools_verbose)
			printf("Created temporary directory %s\n", tmpdir);
	}
	return tmpdir;
}

void keep_temp_dir(void)
{
	tal_del_destructor(temp_dir(), unlink_all);
}

char *temp_file(const void *ctx, const char *extension, const char *srcname)
{
	char *f, *base, *suffix;
	struct stat st;
	unsigned int count = 0;

	base = path_join(ctx, temp_dir(), take(path_basename(ctx, srcname)));
	/* Trim extension. */
	base[path_ext_off(base)] = '\0';
	suffix = tal_strdup(ctx, extension);

	do {
		f = tal_strcat(ctx, base, suffix);
		suffix = tal_fmt(base, "-%u%s", ++count, extension);
	} while (lstat(f, &st) == 0);

	if (tools_verbose)
		printf("Creating file %s\n", f);

	tal_free(base);
	return f;
}

bool move_file(const char *oldname, const char *newname)
{
	char *contents;
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
	contents = grab_file(NULL, oldname);
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

	ret = write_all(fd, contents, tal_count(contents)-1);
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
	tal_free(contents);
	return ret;
}

void *do_tal_realloc(void *p, size_t size)
{
	tal_resize((char **)&p, size);
	return p;
}
