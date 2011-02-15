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
#include <assert.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/failtest/failtest_proto.h>
#include <ccan/failtest/failtest.h>
#include <ccan/build_assert/build_assert.h>

bool (*failtest_hook)(struct failtest_call *history, unsigned num)
= failtest_default_hook;

unsigned int failtest_timeout_ms = 20000;

const char *failpath;

enum info_type {
	WRITE,
	RELEASE_LOCKS,
	FAILURE,
	SUCCESS,
	UNEXPECTED
};

struct write_info_hdr {
	size_t len;
	off_t offset;
	int fd;
};

struct fd_orig {
	int fd;
	off_t offset;
	size_t size;
	bool dupped;
};

struct write_info {
	struct write_info_hdr hdr;
	char *data;
	size_t oldlen;
	char *olddata;
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

static struct write_info *writes = NULL;
static unsigned int writes_num = 0;

static struct write_info *child_writes = NULL;
static unsigned int child_writes_num = 0;

static struct fd_orig *fd_orig = NULL;
static unsigned int fd_orig_num = 0;

static pid_t lock_owner;
static struct lock_info *locks = NULL;
static unsigned int lock_num = 0;

static const char info_to_arg[] = "mceoprwf";

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
	memcpy(&history[history_num].u, elem, elem_size);
	return &history[history_num++];
}

#define add_history(type, file, line, elem) \
	add_history_((type), (file), (line), (elem), sizeof(*(elem)))

static void save_fd_orig(int fd)
{
	unsigned int i;

	for (i = 0; i < fd_orig_num; i++)
		if (fd_orig[i].fd == fd)
			return;

	fd_orig = realloc(fd_orig, (fd_orig_num + 1) * sizeof(*fd_orig));
	fd_orig[fd_orig_num].fd = fd;
	fd_orig[fd_orig_num].dupped = false;
	fd_orig[fd_orig_num].offset = lseek(fd, 0, SEEK_CUR);
	fd_orig[fd_orig_num].size = lseek(fd, 0, SEEK_END);
	lseek(fd, fd_orig[fd_orig_num].offset, SEEK_SET);
	fd_orig_num++;
}

bool failtest_default_hook(struct failtest_call *history, unsigned num)
{
	return true;
}

static bool read_write_info(int fd)
{
	struct write_info_hdr hdr;

	if (!read_all(fd, &hdr, sizeof(hdr)))
		return false;

	child_writes = realloc(child_writes,
			       (child_writes_num+1) * sizeof(child_writes[0]));
	child_writes[child_writes_num].hdr = hdr;
	child_writes[child_writes_num].data = malloc(hdr.len);
	if (!read_all(fd, child_writes[child_writes_num].data, hdr.len))
		return false;

	child_writes_num++;
	return true;
}

static void print_reproduce(void)
{
	unsigned int i;

	printf("To reproduce: --failpath=");
	for (i = 0; i < history_num; i++) {
		if (history[i].fail)
			printf("%c", toupper(info_to_arg[history[i].type]));
		else
			printf("%c", info_to_arg[history[i].type]);
	}
	printf("\n");
}

static void tell_parent(enum info_type type)
{
	if (control_fd != -1)
		write_all(control_fd, &type, sizeof(type));
}

static void child_fail(const char *out, size_t outlen, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%.*s", (int)outlen, out);
	print_reproduce();
	tell_parent(FAILURE);
	exit(1);
}

static pid_t child;

static void hand_down(int signal)
{
	kill(child, signal);
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

static bool should_fail(struct failtest_call *call)
{
	int status;
	int control[2], output[2];
	enum info_type type = UNEXPECTED;
	char *out = NULL;
	size_t outlen = 0;

	if (call == &unrecorded_call)
		return false;

	if (failpath) {
		if (tolower(*failpath) != info_to_arg[call->type])
			errx(1, "Failpath expected '%c' got '%c'\n",
			     info_to_arg[call->type], *failpath);
		call->fail = isupper(*(failpath++));
		return call->fail;
	}

	if (!failtest_hook(history, history_num)) {
		call->fail = false;
		return false;
	}

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

		if (ret <= 0)
			hand_down(SIGUSR1);

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
	if (!WIFEXITED(status))
		child_fail(out, outlen, "Killed by signal %u: ",
			   WTERMSIG(status));
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

	/* We continue onwards without failing. */
	call->fail = false;
	return false;
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
	}
	errno = p->error;
	return p->u.calloc.ret;
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
	}
	errno = p->error;
	return p->u.calloc.ret;
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
		p->u.realloc.ret = realloc(ptr, size);
	}
	errno = p->error;
	return p->u.realloc.ret;
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
		call.mode = va_arg(ap, mode_t);
		va_end(ap);
	}
	p = add_history(FAILTEST_OPEN, file, line, &call);
	/* Avoid memory leak! */
	if (p == &unrecorded_call)
		free((char *)call.pathname);
	if (should_fail(p)) {
		p->u.open.ret = -1;
		/* FIXME: Play with error codes? */
		p->error = EACCES;
	} else {
		p->u.open.ret = open(pathname, call.flags, call.mode);
	}
	errno = p->error;
	return p->u.open.ret;
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

	/* This is going to change seek offset, so save it. */
	if (control_fd != -1)
		save_fd_orig(fd);

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

static struct write_info *new_write(void)
{
	writes = realloc(writes, (writes_num + 1) * sizeof(*writes));
	return &writes[writes_num++];
}

ssize_t failtest_pwrite(int fd, const void *buf, size_t count, off_t off,
			const char *file, unsigned line)
{
	struct failtest_call *p;
	struct write_call call;
	off_t offset;

	call.fd = fd;
	call.buf = buf;
	call.count = count;
	call.off = off;
	p = add_history(FAILTEST_WRITE, file, line, &call);

	offset = lseek(fd, 0, SEEK_CUR);

	/* If we're a child, save contents and tell parent about write. */
	if (control_fd != -1) {
		struct write_info *winfo = new_write();
		enum info_type type = WRITE;

		save_fd_orig(fd);

		winfo->hdr.len = count;
		winfo->hdr.fd = fd;
		winfo->data = malloc(count);
		memcpy(winfo->data, buf, count);
		winfo->hdr.offset = offset;
		if (winfo->hdr.offset != (off_t)-1) {
			lseek(fd, offset, SEEK_SET);
			winfo->olddata = malloc(count);
			winfo->oldlen = read(fd, winfo->olddata, count);
			if (winfo->oldlen == -1)
				winfo->oldlen = 0;
		}
		write_all(control_fd, &type, sizeof(type));
		write_all(control_fd, &winfo->hdr, sizeof(winfo->hdr));
		write_all(control_fd, winfo->data, count);
	}

	/* FIXME: Try partial write returns. */
	if (should_fail(p)) {
		p->u.write.ret = -1;
		p->error = EIO;
	} else {
		/* FIXME: We assume same write order in parent and child */
		if (child_writes_num != 0) {
			if (child_writes[0].hdr.fd != fd)
				errx(1, "Child wrote to fd %u, not %u?",
				     child_writes[0].hdr.fd, fd);
			if (child_writes[0].hdr.offset != offset)
				errx(1, "Child wrote to offset %zu, not %zu?",
				     (size_t)child_writes[0].hdr.offset,
				     (size_t)offset);
			if (child_writes[0].hdr.len != count)
				errx(1, "Child wrote length %zu, not %zu?",
				     child_writes[0].hdr.len, count);
			if (memcmp(child_writes[0].data, buf, count)) {
				child_fail(NULL, 0,
					   "Child wrote differently to"
					   " fd %u than we did!\n", fd);
			}
			free(child_writes[0].data);
			child_writes_num--;
			memmove(&child_writes[0], &child_writes[1],
				sizeof(child_writes[0]) * child_writes_num);

			/* Is this is a socket or pipe, child wrote it
			   already. */
			if (offset == (off_t)-1) {
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

/* We only trap this so we can dup fds in case we need to restore. */
int failtest_close(int fd)
{
	unsigned int i;
	int newfd = -1;

	for (i = 0; i < fd_orig_num; i++) {
		if (fd_orig[i].fd == fd) {
			fd_orig[i].fd = newfd = dup(fd);
			fd_orig[i].dupped = true;
		}
	}

	for (i = 0; i < writes_num; i++) {
		if (writes[i].hdr.fd == fd)
			writes[i].hdr.fd = newfd;
	}

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
	get_locks();

	if (should_fail(p)) {
		p->u.fcntl.ret = -1;
		if (p->u.fcntl.cmd == F_SETLK)
			p->error = EAGAIN;
		else
			p->error = EDEADLK;
	} else {
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

void failtest_init(int argc, char *argv[])
{
	if (argc == 2
	    && strncmp(argv[1], "--failpath=", strlen("--failpath=")) == 0) {
		failpath = argv[1] + strlen("--failpath=");
	}
}

/* Free up memory, so valgrind doesn't report leaks. */
static void free_everything(void)
{
	unsigned int i;

	for (i = 0; i < writes_num; i++) {
		free(writes[i].data);
		if (writes[i].hdr.offset != (off_t)-1)
			free(writes[i].olddata);
	}
	free(writes);
	free(fd_orig);
	for (i = 0; i < history_num; i++) {
		if (history[i].type == FAILTEST_OPEN)
			free((char *)history[i].u.open.pathname);
	}
	free(history);
}

void failtest_exit(int status)
{
	unsigned int i;

	if (control_fd == -1) {
		free_everything();
		exit(status);
	}

	if (failtest_exit_check) {
		if (!failtest_exit_check(history, history_num))
			child_fail(NULL, 0, "failtest_exit_check failed\n");
	}

	/* Restore any stuff we overwrote. */
	for (i = 0; i < writes_num; i++) {
		if (writes[i].hdr.offset == (off_t)-1)
			continue;
		if (writes[i].oldlen != 0) {
			lseek(writes[i].hdr.fd, writes[i].hdr.offset,
			      SEEK_SET);
			write(writes[i].hdr.fd, writes[i].olddata,
			      writes[i].oldlen);
		}
	}

	/* Fix up fd offsets, restore sizes. */
	for (i = 0; i < fd_orig_num; i++) {
		lseek(fd_orig[i].fd, fd_orig[i].offset, SEEK_SET);
		ftruncate(fd_orig[i].fd, fd_orig[i].size);
		/* Free up any file descriptors we dup'ed. */
		if (fd_orig[i].dupped)
			close(fd_orig[i].fd);
	}

	free_everything();
	tell_parent(SUCCESS);
	exit(0);
}
