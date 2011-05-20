#include "config.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <err.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/failtest/failtest_proto.h>
#include <ccan/failtest/failtest.h>
#include <ccan/build_assert/build_assert.h>

enum failtest_result (*failtest_hook)(struct failtest_call *, unsigned);

static int tracefd = -1;

unsigned int failtest_timeout_ms = 20000;

const char *failpath;
const char *debugpath;

enum info_type {
	WRITE,
	RELEASE_LOCKS,
	FAILURE,
	SUCCESS,
	UNEXPECTED
};

struct lock_info {
	int fd;
	/* end is inclusive: you can't have a 0-byte lock. */
	off_t start, end;
	int type;
};

bool (*failtest_exit_check)(struct failtest_call *history, unsigned num);

static struct failtest_call *history = NULL;
static unsigned int history_num = 0;
static int control_fd = -1;
static struct timeval start;
static unsigned int probe_count = 0;

static struct write_call *child_writes = NULL;
static unsigned int child_writes_num = 0;

static pid_t lock_owner;
static struct lock_info *locks = NULL;
static unsigned int lock_num = 0;

static pid_t orig_pid;

static const char info_to_arg[] = "mceoxprwf";

/* Dummy call used for failtest_undo wrappers. */
static struct failtest_call unrecorded_call;

static struct failtest_call *add_history_(enum failtest_call_type type,
					  const char *file,
					  unsigned int line,
					  const void *elem,
					  size_t elem_size)
{
	/* NULL file is how we suppress failure. */
	if (!file)
		return &unrecorded_call;

	history = realloc(history, (history_num + 1) * sizeof(*history));
	history[history_num].type = type;
	history[history_num].file = file;
	history[history_num].line = line;
	history[history_num].cleanup = NULL;
	memcpy(&history[history_num].u, elem, elem_size);
	return &history[history_num++];
}

#define add_history(type, file, line, elem) \
	add_history_((type), (file), (line), (elem), sizeof(*(elem)))

/* We do a fake call inside a sizeof(), to check types. */
#define set_cleanup(call, clean, type)			\
	(call)->cleanup = (void *)((void)sizeof(clean((type *)NULL),1), (clean))

static bool read_write_info(int fd)
{
	struct write_call *w;
	char *buf;

	/* We don't need all of this, but it's simple. */
	child_writes = realloc(child_writes,
			       (child_writes_num+1) * sizeof(child_writes[0]));
	w = &child_writes[child_writes_num];
	if (!read_all(fd, w, sizeof(*w)))
		return false;

	w->buf = buf = malloc(w->count);
	if (!read_all(fd, buf, w->count))
		return false;

	child_writes_num++;
	return true;
}

static char *failpath_string(void)
{
	unsigned int i;
	char *ret = malloc(history_num + 1);

	for (i = 0; i < history_num; i++) {
		ret[i] = info_to_arg[history[i].type];
		if (history[i].fail)
			ret[i] = toupper(ret[i]);
	}
	ret[i] = '\0';
	return ret;
}

static void tell_parent(enum info_type type)
{
	if (control_fd != -1)
		write_all(control_fd, &type, sizeof(type));
}

static void child_fail(const char *out, size_t outlen, const char *fmt, ...)
{
	va_list ap;
	char *path = failpath_string();

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%.*s", (int)outlen, out);
	printf("To reproduce: --failpath=%s\n", path);
	free(path);
	tell_parent(FAILURE);
	exit(1);
}

static void trace(const char *fmt, ...)
{
	va_list ap;

	if (tracefd == -1)
		return;

	va_start(ap, fmt);
	vdprintf(tracefd, fmt, ap);
	va_end(ap);
}

static pid_t child;

static void hand_down(int signum)
{
	kill(child, signum);
}

static void release_locks(void)
{
	/* Locks were never acquired/reacquired? */
	if (lock_owner == 0)
		return;

	/* We own them?  Release them all. */
	if (lock_owner == getpid()) {
		unsigned int i;
		struct flock fl;
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = 0;
		fl.l_len = 0;

		for (i = 0; i < lock_num; i++)
			fcntl(locks[i].fd, F_SETLK, &fl);
	} else {
		/* Our parent must have them; pass request up. */
		enum info_type type = RELEASE_LOCKS;
		assert(control_fd != -1);
		write_all(control_fd, &type, sizeof(type));
	}
	lock_owner = 0;
}

/* off_t is a signed type.  Getting its max is non-trivial. */
static off_t off_max(void)
{
	BUILD_ASSERT(sizeof(off_t) == 4 || sizeof(off_t) == 8);
	if (sizeof(off_t) == 4)
		return (off_t)0x7FFFFFF;
	else
		return (off_t)0x7FFFFFFFFFFFFFFULL;
}

static void get_locks(void)
{
	unsigned int i;
	struct flock fl;

	if (lock_owner == getpid())
		return;

	if (lock_owner != 0) {
		enum info_type type = RELEASE_LOCKS;
		assert(control_fd != -1);
		write_all(control_fd, &type, sizeof(type));
	}

	fl.l_whence = SEEK_SET;

	for (i = 0; i < lock_num; i++) {
		fl.l_type = locks[i].type;
		fl.l_start = locks[i].start;
		if (locks[i].end == off_max())
			fl.l_len = 0;
		else
			fl.l_len = locks[i].end - locks[i].start + 1;

		if (fcntl(locks[i].fd, F_SETLKW, &fl) != 0)
			abort();
	}
	lock_owner = getpid();
}

struct saved_file {
	struct saved_file *next;
	int fd;
	void *contents;
	off_t off, len;
};

static struct saved_file *save_file(struct saved_file *next, int fd)
{
	struct saved_file *s = malloc(sizeof(*s));

	s->next = next;
	s->fd = fd;
	s->off = lseek(fd, 0, SEEK_CUR);
	/* Special file?  Erk... */
	assert(s->off != -1);
	s->len = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	s->contents = malloc(s->len);
	if (read(fd, s->contents, s->len) != s->len)
		err(1, "Failed to save %zu bytes", (size_t)s->len);
	lseek(fd, s->off, SEEK_SET);
	return s;
}
	
/* We have little choice but to save and restore open files: mmap means we
 * can really intercept changes in the child.
 *
 * We could do non-mmap'ed files on demand, however. */
static struct saved_file *save_files(void)
{
	struct saved_file *files = NULL;
	int i;

	/* Figure out the set of live fds. */
	for (i = history_num - 2; i >= 0; i--) {
		if (history[i].type == FAILTEST_OPEN) {
			int fd = history[i].u.open.ret;
			/* Only do successful, writable fds. */
			if (fd < 0)
				continue;

			/* If it was closed, cleanup == NULL. */
			if (!history[i].cleanup)
				continue;

			if ((history[i].u.open.flags & O_RDWR) == O_RDWR) {
				files = save_file(files, fd);
			} else if ((history[i].u.open.flags & O_WRONLY)
				   == O_WRONLY) {
				/* FIXME: Handle O_WRONLY.  Open with O_RDWR? */
				abort();
			}
		}
	}

	return files;
}

static void restore_files(struct saved_file *s)
{
	while (s) {
		struct saved_file *next = s->next;

		lseek(s->fd, 0, SEEK_SET);
		if (write(s->fd, s->contents, s->len) != s->len)
			err(1, "Failed to restore %zu bytes", (size_t)s->len);
		if (ftruncate(s->fd, s->len) != 0)
			err(1, "Failed to trim file to length %zu",
			    (size_t)s->len);
		free(s->contents);
		lseek(s->fd, s->off, SEEK_SET);
		free(s);
		s = next;
	}
}

/* Free up memory, so valgrind doesn't report leaks. */
static void free_everything(void)
{
	unsigned int i;

	/* We don't do this in cleanup: needed even for failed opens. */
	for (i = 0; i < history_num; i++) {
		if (history[i].type == FAILTEST_OPEN)
			free((char *)history[i].u.open.pathname);
	}
	free(history);
}

static NORETURN void failtest_cleanup(bool forced_cleanup, int status)
{
	int i;

	/* For children, we don't care if they "failed" the testing. */
	if (control_fd != -1)
		status = 0;

	if (forced_cleanup)
		history_num--;

	/* Cleanup everything, in reverse order. */
	for (i = history_num - 1; i >= 0; i--) {
		if (!history[i].cleanup)
			continue;
		if (!forced_cleanup) {
			printf("Leak at %s:%u: --failpath=%s\n",
			       history[i].file, history[i].line,
			       failpath_string());
			status = 1;
		}
		history[i].cleanup(&history[i].u);
	}

	free_everything();
	tell_parent(SUCCESS);
	exit(status);
}

static bool should_fail(struct failtest_call *call)
{
	int status;
	int control[2], output[2];
	enum info_type type = UNEXPECTED;
	char *out = NULL;
	size_t outlen = 0;
	struct saved_file *files;

	/* Are we probing? */
	if (probe_count && --probe_count == 0)
		failtest_cleanup(true, 0);

	if (call == &unrecorded_call)
		return false;

	if (failpath) {
		/* + means continue after end, like normal. */
		if (*failpath == '+')
			failpath = NULL;
		else if (*failpath == '\0') {
			/* Continue, but don't inject errors. */
			return call->fail = false;
		} else {
			if (tolower((unsigned char)*failpath)
			    != info_to_arg[call->type])
				errx(1, "Failpath expected '%c' got '%c'\n",
				     info_to_arg[call->type], *failpath);
			call->fail = isupper((unsigned char)*(failpath++));
			return call->fail;
		}
	}

	/* Attach debugger if they asked for it. */
	if (debugpath && history_num == strlen(debugpath)) {
		unsigned int i;

		for (i = 0; i < history_num; i++) {
			unsigned char c = info_to_arg[history[i].type];
			if (history[i].fail)
				c = toupper(c);
			if (c != debugpath[i])
				break;
		}
		if (i == history_num) {
			char str[80];

			/* Don't timeout. */
			signal(SIGUSR1, SIG_IGN);
			sprintf(str, "xterm -e gdb /proc/%d/exe %d &",
				getpid(), getpid());
			if (system(str) == 0)
				sleep(5);
		}
	}

	if (failtest_hook) {
		switch (failtest_hook(history, history_num)) {
		case FAIL_OK:
			break;
		case FAIL_DONT_FAIL:
			call->fail = false;
			return false;
		case FAIL_PROBE:
			/* Already down probe path?  Stop now. */
			if (probe_count)
				failtest_cleanup(true, 0);
			/* FIXME: We should run *parent* and run probe until
			 * calls match up again. */
			probe_count = 3;
			break;
		default:
			abort();
		}
	}

	files = save_files();

	/* We're going to fail in the child. */
	call->fail = true;
	if (pipe(control) != 0 || pipe(output) != 0)
		err(1, "opening pipe");

	/* Prevent double-printing (in child and parent) */
	fflush(stdout);
	child = fork();
	if (child == -1)
		err(1, "forking failed");

	if (child == 0) {
		if (tracefd != -1) {
			struct timeval now;
			const char *p;
			gettimeofday(&now, NULL);
			if (now.tv_usec < start.tv_usec) {
				now.tv_sec--;
				now.tv_usec += 1000000;
			}
			now.tv_usec -= start.tv_usec;
			now.tv_sec -= start.tv_sec;
			p = failpath_string();
			trace("%u->%u (%u.%02u): %s (", getppid(), getpid(),
			      (int)now.tv_sec, (int)now.tv_usec / 10000, p);
			free((char *)p);
			p = strrchr(history[history_num-1].file, '/');
			if (p)
				trace("%s", p+1);
			else
				trace("%s", history[history_num-1].file);
			trace(":%u)\n", history[history_num-1].line);
		}
		close(control[0]);
		close(output[0]);
		dup2(output[1], STDOUT_FILENO);
		dup2(output[1], STDERR_FILENO);
		if (output[1] != STDOUT_FILENO && output[1] != STDERR_FILENO)
			close(output[1]);
		control_fd = control[1];
		return true;
	}

	signal(SIGUSR1, hand_down);

	close(control[1]);
	close(output[1]);

	/* We grab output so we can display it; we grab writes so we
	 * can compare. */
	do {
		struct pollfd pfd[2];
		int ret;

		pfd[0].fd = output[0];
		pfd[0].events = POLLIN|POLLHUP;
		pfd[1].fd = control[0];
		pfd[1].events = POLLIN|POLLHUP;

		if (type == SUCCESS)
			ret = poll(pfd, 1, failtest_timeout_ms);
		else
			ret = poll(pfd, 2, failtest_timeout_ms);

		if (ret == 0)
			hand_down(SIGUSR1);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			err(1, "Poll returned %i", ret);
		}

		if (pfd[0].revents & POLLIN) {
			ssize_t len;

			out = realloc(out, outlen + 8192);
			len = read(output[0], out + outlen, 8192);
			outlen += len;
		} else if (type != SUCCESS && (pfd[1].revents & POLLIN)) {
			if (read_all(control[0], &type, sizeof(type))) {
				if (type == WRITE) {
					if (!read_write_info(control[0]))
						break;
				} else if (type == RELEASE_LOCKS) {
					release_locks();
					/* FIXME: Tell them we're done... */
				}
			}
		} else if (pfd[0].revents & POLLHUP) {
			break;
		}
	} while (type != FAILURE);

	close(output[0]);
	close(control[0]);
	waitpid(child, &status, 0);
	if (!WIFEXITED(status)) {
		if (WTERMSIG(status) == SIGUSR1)
			child_fail(out, outlen, "Timed out");
		else
			child_fail(out, outlen, "Killed by signal %u: ",
				   WTERMSIG(status));
	}
	/* Child printed failure already, just pass up exit code. */
	if (type == FAILURE) {
		fprintf(stderr, "%.*s", (int)outlen, out);
		tell_parent(type);
		exit(WEXITSTATUS(status) ? WEXITSTATUS(status) : 1);
	}
	if (WEXITSTATUS(status) != 0)
		child_fail(out, outlen, "Exited with status %i: ",
			   WEXITSTATUS(status));

	free(out);
	signal(SIGUSR1, SIG_DFL);

	restore_files(files);

	/* We continue onwards without failing. */
	call->fail = false;
	return false;
}

static void cleanup_calloc(struct calloc_call *call)
{
	free(call->ret);
}

void *failtest_calloc(size_t nmemb, size_t size,
		      const char *file, unsigned line)
{
	struct failtest_call *p;
	struct calloc_call call;
	call.nmemb = nmemb;
	call.size = size;
	p = add_history(FAILTEST_CALLOC, file, line, &call);

	if (should_fail(p)) {
		p->u.calloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		p->u.calloc.ret = calloc(nmemb, size);
		set_cleanup(p, cleanup_calloc, struct calloc_call);
	}
	errno = p->error;
	return p->u.calloc.ret;
}

static void cleanup_malloc(struct malloc_call *call)
{
	free(call->ret);
}

void *failtest_malloc(size_t size, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct malloc_call call;
	call.size = size;

	p = add_history(FAILTEST_MALLOC, file, line, &call);
	if (should_fail(p)) {
		p->u.calloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		p->u.calloc.ret = malloc(size);
		set_cleanup(p, cleanup_malloc, struct malloc_call);
	}
	errno = p->error;
	return p->u.calloc.ret;
}

static void cleanup_realloc(struct realloc_call *call)
{
	free(call->ret);
}

/* Walk back and find out if we got this ptr from a previous routine. */
static void fixup_ptr_history(void *ptr, unsigned int last)
{
	int i;

	/* Start at end of history, work back. */
	for (i = last - 1; i >= 0; i--) {
		switch (history[i].type) {
		case FAILTEST_REALLOC:
			if (history[i].u.realloc.ret == ptr) {
				history[i].cleanup = NULL;
				return;
			}
			break;
		case FAILTEST_MALLOC:
			if (history[i].u.malloc.ret == ptr) {
				history[i].cleanup = NULL;
				return;
			}
			break;
		case FAILTEST_CALLOC:
			if (history[i].u.calloc.ret == ptr) {
				history[i].cleanup = NULL;
				return;
			}
			break;
		default:
			break;
		}
	}
}

void *failtest_realloc(void *ptr, size_t size, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct realloc_call call;
	call.size = size;
	p = add_history(FAILTEST_REALLOC, file, line, &call);

	/* FIXME: Try one child moving allocation, one not. */
	if (should_fail(p)) {
		p->u.realloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		fixup_ptr_history(ptr, history_num-1);
		p->u.realloc.ret = realloc(ptr, size);
		set_cleanup(p, cleanup_realloc, struct realloc_call);
	}
	errno = p->error;
	return p->u.realloc.ret;
}

void failtest_free(void *ptr)
{
	fixup_ptr_history(ptr, history_num);
	free(ptr);
}

static void cleanup_open(struct open_call *call)
{
	close(call->ret);
}

int failtest_open(const char *pathname,
		  const char *file, unsigned line, ...)
{
	struct failtest_call *p;
	struct open_call call;
	va_list ap;

	call.pathname = strdup(pathname);
	va_start(ap, line);
	call.flags = va_arg(ap, int);
	if (call.flags & O_CREAT) {
		call.mode = va_arg(ap, int);
		va_end(ap);
	}
	p = add_history(FAILTEST_OPEN, file, line, &call);
	/* Avoid memory leak! */
	if (p == &unrecorded_call)
		free((char *)call.pathname);
	p->u.open.ret = open(pathname, call.flags, call.mode);

	if (!failpath && p->u.open.ret == -1) {
		p->fail = false;
		p->error = errno;
	} else if (should_fail(p)) {
		close(p->u.open.ret);
		p->u.open.ret = -1;
		/* FIXME: Play with error codes? */
		p->error = EACCES;
	} else {
		set_cleanup(p, cleanup_open, struct open_call);
	}
	errno = p->error;
	return p->u.open.ret;
}

static void cleanup_pipe(struct pipe_call *call)
{
	if (!call->closed[0])
		close(call->fds[0]);
	if (!call->closed[1])
		close(call->fds[1]);
}

int failtest_pipe(int pipefd[2], const char *file, unsigned line)
{
	struct failtest_call *p;
	struct pipe_call call;

	p = add_history(FAILTEST_PIPE, file, line, &call);
	if (should_fail(p)) {
		p->u.open.ret = -1;
		/* FIXME: Play with error codes? */
		p->error = EMFILE;
	} else {
		p->u.pipe.ret = pipe(p->u.pipe.fds);
		p->u.pipe.closed[0] = p->u.pipe.closed[1] = false;
		set_cleanup(p, cleanup_pipe, struct pipe_call);
	}
	/* This causes valgrind to notice if they use pipefd[] after failure */
	memcpy(pipefd, p->u.pipe.fds, sizeof(p->u.pipe.fds));
	errno = p->error;
	return p->u.pipe.ret;
}

ssize_t failtest_pread(int fd, void *buf, size_t count, off_t off,
		       const char *file, unsigned line)
{
	struct failtest_call *p;
	struct read_call call;
	call.fd = fd;
	call.buf = buf;
	call.count = count;
	call.off = off;
	p = add_history(FAILTEST_READ, file, line, &call);

	/* FIXME: Try partial read returns. */
	if (should_fail(p)) {
		p->u.read.ret = -1;
		p->error = EIO;
	} else {
		p->u.read.ret = pread(fd, buf, count, off);
	}
	errno = p->error;
	return p->u.read.ret;
}

ssize_t failtest_pwrite(int fd, const void *buf, size_t count, off_t off,
			const char *file, unsigned line)
{
	struct failtest_call *p;
	struct write_call call;

	call.fd = fd;
	call.buf = buf;
	call.count = count;
	call.off = off;
	p = add_history(FAILTEST_WRITE, file, line, &call);

	/* If we're a child, we need to make sure we write the same thing
	 * to non-files as the parent does, so tell it. */
	if (control_fd != -1 && off == (off_t)-1) {
		enum info_type type = WRITE;

		write_all(control_fd, &type, sizeof(type));
		write_all(control_fd, &p->u.write, sizeof(p->u.write));
		write_all(control_fd, buf, count);
	}

	/* FIXME: Try partial write returns. */
	if (should_fail(p)) {
		p->u.write.ret = -1;
		p->error = EIO;
	} else {
		/* FIXME: We assume same write order in parent and child */
		if (off == (off_t)-1 && child_writes_num != 0) {
			if (child_writes[0].fd != fd)
				errx(1, "Child wrote to fd %u, not %u?",
				     child_writes[0].fd, fd);
			if (child_writes[0].off != p->u.write.off)
				errx(1, "Child wrote to offset %zu, not %zu?",
				     (size_t)child_writes[0].off,
				     (size_t)p->u.write.off);
			if (child_writes[0].count != count)
				errx(1, "Child wrote length %zu, not %zu?",
				     child_writes[0].count, count);
			if (memcmp(child_writes[0].buf, buf, count)) {
				child_fail(NULL, 0,
					   "Child wrote differently to"
					   " fd %u than we did!\n", fd);
			}
			free((char *)child_writes[0].buf);
			child_writes_num--;
			memmove(&child_writes[0], &child_writes[1],
				sizeof(child_writes[0]) * child_writes_num);

			/* Is this is a socket or pipe, child wrote it
			   already. */
			if (p->u.write.off == (off_t)-1) {
				p->u.write.ret = count;
				errno = p->error;
				return p->u.write.ret;
			}
		}
		p->u.write.ret = pwrite(fd, buf, count, off);
	}
	errno = p->error;
	return p->u.write.ret;
}

ssize_t failtest_read(int fd, void *buf, size_t count,
		      const char *file, unsigned line)
{
	return failtest_pread(fd, buf, count, lseek(fd, 0, SEEK_CUR),
			      file, line);
}

ssize_t failtest_write(int fd, const void *buf, size_t count,
		       const char *file, unsigned line)
{
	return failtest_pwrite(fd, buf, count, lseek(fd, 0, SEEK_CUR),
			       file, line);
}

static struct lock_info *WARN_UNUSED_RESULT
add_lock(struct lock_info *locks, int fd, off_t start, off_t end, int type)
{
	unsigned int i;
	struct lock_info *l;

	for (i = 0; i < lock_num; i++) {
		l = &locks[i];

		if (l->fd != fd)
			continue;
		/* Four cases we care about:
		 * Start overlap:
		 *	l =    |      |
		 *	new = |   |
		 * Mid overlap:
		 *	l =    |      |
		 *	new =    |  |
		 * End overlap:
		 *	l =    |      |
		 *	new =      |    |
		 * Total overlap:
		 *	l =    |      |
		 *	new = |         |
		 */
		if (start > l->start && end < l->end) {
			/* Mid overlap: trim entry, add new one. */
			off_t new_start, new_end;
			new_start = end + 1;
			new_end = l->end;
			l->end = start - 1;
			locks = add_lock(locks,
					 fd, new_start, new_end, l->type);
			l = &locks[i];
		} else if (start <= l->start && end >= l->end) {
			/* Total overlap: eliminate entry. */
			l->end = 0;
			l->start = 1;
		} else if (end >= l->start && end < l->end) {
			/* Start overlap: trim entry. */
			l->start = end + 1;
		} else if (start > l->start && start <= l->end) {
			/* End overlap: trim entry. */
			l->end = start-1;
		}
		/* Nothing left?  Remove it. */
		if (l->end < l->start) {
			memmove(l, l + 1, (--lock_num - i) * sizeof(l[0]));
			i--;
		}
	}

	if (type != F_UNLCK) {
		locks = realloc(locks, (lock_num + 1) * sizeof(*locks));
		l = &locks[lock_num++];
		l->fd = fd;
		l->start = start;
		l->end = end;
		l->type = type;
	}
	return locks;
}

/* We trap this so we can record it: we don't fail it. */
int failtest_close(int fd, const char *file, unsigned line)
{
	int i;
	struct close_call call;
	struct failtest_call *p;

	call.fd = fd;
	p = add_history(FAILTEST_CLOSE, file, line, &call);
	p->fail = false;

	/* Consume close from failpath. */
	if (failpath)
		if (should_fail(p))
			abort();

	if (fd < 0)
		return close(fd);

	/* Trace history to find source of fd. */
	for (i = history_num-1; i >= 0; i--) {
		switch (history[i].type) {
		case FAILTEST_PIPE:
			/* From a pipe? */
			if (history[i].u.pipe.fds[0] == fd) {
				assert(!history[i].u.pipe.closed[0]);
				history[i].u.pipe.closed[0] = true;
				if (history[i].u.pipe.closed[1])
					history[i].cleanup = NULL;
				goto out;
			}
			if (history[i].u.pipe.fds[1] == fd) {
				assert(!history[i].u.pipe.closed[1]);
				history[i].u.pipe.closed[1] = true;
				if (history[i].u.pipe.closed[0])
					history[i].cleanup = NULL;
				goto out;
			}
			break;
		case FAILTEST_OPEN:
			if (history[i].u.open.ret == fd) {
				assert((void *)history[i].cleanup
				       == (void *)cleanup_open);
				history[i].cleanup = NULL;
				goto out;
			}
			break;
		default:
			break;
		}
	}

out:
	locks = add_lock(locks, fd, 0, off_max(), F_UNLCK);
	return close(fd);
}

/* Zero length means "to end of file" */
static off_t end_of(off_t start, off_t len)
{
	if (len == 0)
		return off_max();
	return start + len - 1;
}

/* FIXME: This only handles locks, really. */
int failtest_fcntl(int fd, const char *file, unsigned line, int cmd, ...)
{
	struct failtest_call *p;
	struct fcntl_call call;
	va_list ap;

	call.fd = fd;
	call.cmd = cmd;

	/* Argument extraction. */
	switch (cmd) {
	case F_SETFL:
	case F_SETFD:
		va_start(ap, cmd);
		call.arg.l = va_arg(ap, long);
		va_end(ap);
		return fcntl(fd, cmd, call.arg.l);
	case F_GETFD:
	case F_GETFL:
		return fcntl(fd, cmd);
	case F_GETLK:
		get_locks();
		va_start(ap, cmd);
		call.arg.fl = *va_arg(ap, struct flock *);
		va_end(ap);
		return fcntl(fd, cmd, &call.arg.fl);
	case F_SETLK:
	case F_SETLKW:
		va_start(ap, cmd);
		call.arg.fl = *va_arg(ap, struct flock *);
		va_end(ap);
		break;
	default:
		/* This means you need to implement it here. */
		err(1, "failtest: unknown fcntl %u", cmd);
	}

	p = add_history(FAILTEST_FCNTL, file, line, &call);

	if (should_fail(p)) {
		p->u.fcntl.ret = -1;
		if (p->u.fcntl.cmd == F_SETLK)
			p->error = EAGAIN;
		else
			p->error = EDEADLK;
	} else {
		get_locks();
		p->u.fcntl.ret = fcntl(p->u.fcntl.fd, p->u.fcntl.cmd,
				       &p->u.fcntl.arg.fl);
		if (p->u.fcntl.ret == -1)
			p->error = errno;
		else {
			/* We don't handle anything else yet. */
			assert(p->u.fcntl.arg.fl.l_whence == SEEK_SET);
			locks = add_lock(locks,
					 p->u.fcntl.fd,
					 p->u.fcntl.arg.fl.l_start,
					 end_of(p->u.fcntl.arg.fl.l_start,
						p->u.fcntl.arg.fl.l_len),
					 p->u.fcntl.arg.fl.l_type);
		}
	}
	errno = p->error;
	return p->u.fcntl.ret;
}

pid_t failtest_getpid(const char *file, unsigned line)
{
	/* You must call failtest_init first! */
	assert(orig_pid);
	return orig_pid;
}
	
void failtest_init(int argc, char *argv[])
{
	unsigned int i;

	orig_pid = getpid();
		
	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--failpath=", strlen("--failpath="))) {
			failpath = argv[i] + strlen("--failpath=");
		} else if (strcmp(argv[i], "--tracepath") == 0) {
			tracefd = dup(STDERR_FILENO);
			failtest_timeout_ms = -1;
		} else if (!strncmp(argv[i], "--debugpath=",
				    strlen("--debugpath="))) {
			debugpath = argv[i] + strlen("--debugpath=");
		}
	}
	gettimeofday(&start, NULL);
}

void failtest_exit(int status)
{
	if (failtest_exit_check) {
		if (!failtest_exit_check(history, history_num))
			child_fail(NULL, 0, "failtest_exit_check failed\n");
	}

	failtest_cleanup(false, status);
}
