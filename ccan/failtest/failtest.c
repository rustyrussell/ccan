/* Licensed under LGPL - see LICENSE file for details */
#include <ccan/failtest/failtest.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <signal.h>
#include <assert.h>
#include <ccan/err/err.h>
#include <ccan/time/time.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/failtest/failtest_proto.h>
#include <ccan/build_assert/build_assert.h>
#include <ccan/hash/hash.h>
#include <ccan/htable/htable_type.h>
#include <ccan/str/str.h>
#include <ccan/compiler/compiler.h>

enum failtest_result (*failtest_hook)(struct tlist_calls *);

static FILE *tracef = NULL, *warnf;
static int traceindent = 0;

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

/* We hash the call location together with its backtrace. */
static size_t hash_call(const struct failtest_call *call)
{
	return hash(call->file, strlen(call->file),
		    hash(&call->line, 1,
			 hash(call->backtrace, call->backtrace_num,
			      call->type)));
}

static bool call_eq(const struct failtest_call *call1,
		    const struct failtest_call *call2)
{
	unsigned int i;

	if (strcmp(call1->file, call2->file) != 0
	    || call1->line != call2->line
	    || call1->type != call2->type
	    || call1->backtrace_num != call2->backtrace_num)
		return false;

	for (i = 0; i < call1->backtrace_num; i++)
		if (call1->backtrace[i] != call2->backtrace[i])
			return false;

	return true;
}

/* Defines struct failtable. */
HTABLE_DEFINE_TYPE(struct failtest_call, (struct failtest_call *), hash_call,
		   call_eq, failtable);

bool (*failtest_exit_check)(struct tlist_calls *history);

/* The entire history of all calls. */
static struct tlist_calls history = TLIST_INIT(history);
/* If we're a child, the fd two write control info to the parent. */
static int control_fd = -1;
/* If we're a child, this is the first call we did ourselves. */
static struct failtest_call *our_history_start = NULL;
/* For printing runtime with --trace. */
static struct timeabs start;
/* Set when failtest_hook returns FAIL_PROBE */
static bool probing = false;
/* Table to track duplicates. */
static struct failtable failtable;

/* Array of writes which our child did.  We report them on failure. */
static struct write_call *child_writes = NULL;
static unsigned int child_writes_num = 0;

/* fcntl locking info. */
static pid_t lock_owner;
static struct lock_info *locks = NULL;
static unsigned int lock_num = 0;

/* Our original pid, which we return to anyone who asks. */
static pid_t orig_pid;

/* Mapping from failtest_type to char. */
static const char info_to_arg[] = "mceoxprwfal";

/* Dummy call used for failtest_undo wrappers. */
static struct failtest_call unrecorded_call;

struct contents_saved {
	size_t count;
	off_t off;
	off_t old_len;
	char contents[1];
};

/* File contents, saved in this child only. */
struct saved_mmapped_file {
	struct saved_mmapped_file *next;
	struct failtest_call *opener;
	struct contents_saved *s;
};

static struct saved_mmapped_file *saved_mmapped_files;

#if HAVE_BACKTRACE
#include <execinfo.h>

static void **get_backtrace(unsigned int *num)
{
	static unsigned int max_back = 100;
	void **ret;

again:
	ret = malloc(max_back * sizeof(void *));
	*num = backtrace(ret, max_back);
	if (*num == max_back) {
		free(ret);
		max_back *= 2;
		goto again;
	}
	return ret;
}
#else
/* This will test slightly less, since will consider all of the same
 * calls as identical.  But, it's slightly faster! */
static void **get_backtrace(unsigned int *num)
{
	*num = 0;
	return NULL;
}
#endif /* HAVE_BACKTRACE */

static struct failtest_call *add_history_(enum failtest_call_type type,
					  bool can_leak,
					  const char *file,
					  unsigned int line,
					  const void *elem,
					  size_t elem_size)
{
	struct failtest_call *call;

	/* NULL file is how we suppress failure. */
	if (!file)
		return &unrecorded_call;

	call = malloc(sizeof *call);
	call->type = type;
	call->can_leak = can_leak;
	call->file = file;
	call->line = line;
	call->cleanup = NULL;
	call->backtrace = get_backtrace(&call->backtrace_num);
	memcpy(&call->u, elem, elem_size);
	tlist_add_tail(&history, call, list);
	return call;
}

#define add_history(type, can_leak, file, line, elem)		\
	add_history_((type), (can_leak), (file), (line), (elem), sizeof(*(elem)))

/* We do a fake call inside a sizeof(), to check types. */
#define set_cleanup(call, clean, type)			\
	(call)->cleanup = (void *)((void)sizeof(clean((type *)NULL, false),1), (clean))

/* Dup the fd to a high value (out of the way I hope!), and close the old fd. */
static int move_fd_to_high(int fd)
{
	int i;
	struct rlimit lim;
	int max;

	if (getrlimit(RLIMIT_NOFILE, &lim) == 0) {
		max = lim.rlim_cur;
		printf("Max is %i\n", max);
	} else
		max = FD_SETSIZE;

	for (i = max - 1; i > fd; i--) {
		if (fcntl(i, F_GETFL) == -1 && errno == EBADF) {
			if (dup2(fd, i) == -1) {
				warn("Failed to dup fd %i to %i", fd, i);
				continue;
			}
			close(fd);
			return i;
		}
	}
	/* Nothing?  Really?  Er... ok? */
	return fd;
}

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
	struct failtest_call *i;
	char *ret = strdup("");
	unsigned len = 0;

	/* Inefficient, but who cares? */
	tlist_for_each(&history, i, list) {
		ret = realloc(ret, len + 2);
		ret[len] = info_to_arg[i->type];
		if (i->fail)
			ret[len] = toupper(ret[len]);
		ret[++len] = '\0';
	}
	return ret;
}

static void do_warn(int e, const char *fmt, va_list ap)
{
	char *p = failpath_string();

	vfprintf(warnf, fmt, ap);
	if (e != -1)
		fprintf(warnf, ": %s", strerror(e));
	fprintf(warnf, " [%s]\n", p);
	free(p);
}

static void fwarn(const char *fmt, ...)
{
	va_list ap;
	int e = errno;

	va_start(ap, fmt);
	do_warn(e, fmt, ap);
	va_end(ap);
}


static void fwarnx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	do_warn(-1, fmt, ap);
	va_end(ap);
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

static void PRINTF_FMT(1, 2) trace(const char *fmt, ...)
{
	va_list ap;
	unsigned int i;
	char *p;
	static int idx;

	if (!tracef)
		return;

	for (i = 0; i < traceindent; i++)
		fprintf(tracef, "  ");

	p = failpath_string();
	fprintf(tracef, "%i: %u: %s ", idx++, getpid(), p);
	va_start(ap, fmt);
	vfprintf(tracef, fmt, ap);
	va_end(ap);
	free(p);
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

		trace("Releasing %u locks\n", lock_num);
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
		trace("Asking parent to release locks\n");
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
	trace("Acquired %u locks\n", lock_num);
	lock_owner = getpid();
}


static struct contents_saved *save_contents(const char *filename,
					    int fd, size_t count, off_t off,
					    const char *why)
{
	struct contents_saved *s = malloc(sizeof(*s) + count);
	ssize_t ret;

	s->off = off;

	ret = pread(fd, s->contents, count, off);
	if (ret < 0) {
		fwarn("failtest_write: failed to save old contents!");
		s->count = 0;
	} else
		s->count = ret;

	/* Use lseek to get the size of file, but we have to restore
	 * file offset */
	off = lseek(fd, 0, SEEK_CUR);
	s->old_len = lseek(fd, 0, SEEK_END);
	lseek(fd, off, SEEK_SET);

	trace("Saving %p %s %zu@%llu after %s (filelength %llu) via fd %i\n",
	      s, filename, s->count, (long long)s->off, why,
	      (long long)s->old_len, fd);
	return s;
}

static void restore_contents(struct failtest_call *opener,
			     struct contents_saved *s,
			     bool restore_offset,
			     const char *caller)
{
	int fd;

	/* The top parent doesn't need to restore. */
	if (control_fd == -1)
		return;

	/* Has the fd been closed? */
	if (opener->u.open.closed) {
		/* Reopen, replace fd, close silently as we clean up. */
		fd = open(opener->u.open.pathname, O_RDWR);
		if (fd < 0) {
			fwarn("failtest: could not reopen %s to clean up %s!",
			      opener->u.open.pathname, caller);
			return;
		}
		/* Make it clearly distinguisable from a "normal" fd. */
		fd = move_fd_to_high(fd);
		trace("Reopening %s to restore it (was fd %i, now %i)\n",
		      opener->u.open.pathname, opener->u.open.ret, fd);
		opener->u.open.ret = fd;
		opener->u.open.closed = false;
	}
	fd = opener->u.open.ret;

	trace("Restoring %p %s %zu@%llu after %s (filelength %llu) via fd %i\n",
	      s, opener->u.open.pathname, s->count, (long long)s->off, caller,
	      (long long)s->old_len, fd);
	if (pwrite(fd, s->contents, s->count, s->off) != s->count) {
		fwarn("failtest: write failed cleaning up %s for %s!",
		      opener->u.open.pathname, caller);
	}

	if (ftruncate(fd, s->old_len) != 0) {
		fwarn("failtest_write: truncate failed cleaning up %s for %s!",
		      opener->u.open.pathname, caller);
	}

	if (restore_offset) {
		trace("Restoring offset of fd %i to %llu\n",
		      fd, (long long)s->off);
		lseek(fd, s->off, SEEK_SET);
	}
}

/* We save/restore most things on demand, but always do mmaped files. */
static void save_mmapped_files(void)
{
	struct failtest_call *i;
	trace("Saving mmapped files in child\n");

	tlist_for_each_rev(&history, i, list) {
		struct mmap_call *m = &i->u.mmap;
		struct saved_mmapped_file *s;

		if (i->type != FAILTEST_MMAP)
			continue;

		/* FIXME: We only handle mmapped files where fd is still open. */
		if (m->opener->u.open.closed)
			continue;

		s = malloc(sizeof *s);
		s->s = save_contents(m->opener->u.open.pathname,
				     m->fd, m->length, m->offset,
				     "mmapped file before fork");
		s->opener = m->opener;
		s->next = saved_mmapped_files;
		saved_mmapped_files = s;
	}
}

static void free_mmapped_files(bool restore)
{
	trace("%s mmapped files in child\n",
	      restore ? "Restoring" : "Discarding");
	while (saved_mmapped_files) {
		struct saved_mmapped_file *next = saved_mmapped_files->next;
		if (restore)
			restore_contents(saved_mmapped_files->opener,
					 saved_mmapped_files->s, false,
					 "saved mmap");
		free(saved_mmapped_files->s);
		free(saved_mmapped_files);
		saved_mmapped_files = next;
	}
}

/* Returns a FAILTEST_OPEN, FAILTEST_PIPE or NULL. */
static struct failtest_call *opener_of(int fd)
{
	struct failtest_call *i;

	/* Don't get confused and match genuinely failed opens. */
	if (fd < 0)
		return NULL;

	/* Figure out the set of live fds. */
	tlist_for_each_rev(&history, i, list) {
		if (i->fail)
			continue;
		switch (i->type) {
		case FAILTEST_CLOSE:
			if (i->u.close.fd == fd) {
				return NULL;
			}
			break;
		case FAILTEST_OPEN:
			if (i->u.open.ret == fd) {
				if (i->u.open.closed)
					return NULL;
				return i;
			}
			break;
		case FAILTEST_PIPE:
			if (i->u.pipe.fds[0] == fd || i->u.pipe.fds[1] == fd) {
				return i;
			}
			break;
		default:
			break;
		}
	}

	/* FIXME: socket, dup, etc are untracked! */
	return NULL;
}

static void free_call(struct failtest_call *call)
{
	/* We don't do this in cleanup: needed even for failed opens. */
	if (call->type == FAILTEST_OPEN)
		free((char *)call->u.open.pathname);
	free(call->backtrace);
	tlist_del_from(&history, call, list);
	free(call);
}

/* Free up memory, so valgrind doesn't report leaks. */
static void free_everything(void)
{
	struct failtest_call *i;

	while ((i = tlist_top(&history, list)) != NULL)
		free_call(i);

	failtable_clear(&failtable);
}

static NORETURN void failtest_cleanup(bool forced_cleanup, int status)
{
	struct failtest_call *i;
	bool restore = true;

	/* For children, we don't care if they "failed" the testing. */
	if (control_fd != -1)
		status = 0;
	else
		/* We don't restore contents for original parent. */
		restore = false;

	/* Cleanup everything, in reverse order. */
	tlist_for_each_rev(&history, i, list) {
		/* Don't restore things our parent did. */
		if (i == our_history_start)
			restore = false;

		if (i->fail)
			continue;

		if (i->cleanup)
			i->cleanup(&i->u, restore);

		/* But their program shouldn't leak, even on failure. */
		if (!forced_cleanup && i->can_leak) {
			char *p = failpath_string();
			printf("Leak at %s:%u: --failpath=%s\n",
			       i->file, i->line, p);
			free(p);
			status = 1;
		}
	}

	/* Put back mmaped files the way our parent (if any) expects. */
	free_mmapped_files(true);

	free_everything();
	if (status == 0)
		tell_parent(SUCCESS);
	else
		tell_parent(FAILURE);
	exit(status);
}

static bool following_path(void)
{
	if (!failpath)
		return false;
	/* + means continue after end, like normal. */
	if (*failpath == '+') {
		failpath = NULL;
		return false;
	}
	return true;
}

static bool follow_path(struct failtest_call *call)
{
	if (*failpath == '\0') {
		/* Continue, but don't inject errors. */
		return call->fail = false;
	}

	if (tolower((unsigned char)*failpath) != info_to_arg[call->type])
		errx(1, "Failpath expected '%s' got '%c'\n",
		     failpath, info_to_arg[call->type]);
	call->fail = cisupper(*(failpath++));
			if (call->fail)
				call->can_leak = false;
	return call->fail;
}

static bool should_fail(struct failtest_call *call)
{
	int status;
	int control[2], output[2];
	enum info_type type = UNEXPECTED;
	char *out = NULL;
	size_t outlen = 0;
	struct failtest_call *dup;

	if (call == &unrecorded_call)
		return false;

	if (following_path())
		return follow_path(call);

	/* Attach debugger if they asked for it. */
	if (debugpath) {
		char *path;

		/* Pretend this last call matches whatever path wanted:
		 * keeps valgrind happy. */
		call->fail = cisupper(debugpath[strlen(debugpath)-1]);
		path = failpath_string();

		if (streq(path, debugpath)) {
			char str[80];

			/* Don't timeout. */
			signal(SIGUSR1, SIG_IGN);
			sprintf(str, "xterm -e gdb /proc/%d/exe %d &",
				getpid(), getpid());
			if (system(str) == 0)
				sleep(5);
		} else {
			/* Ignore last character: could be upper or lower. */
			path[strlen(path)-1] = '\0';
			if (!strstarts(debugpath, path)) {
				fprintf(stderr,
					"--debugpath not followed: %s\n", path);
				debugpath = NULL;
			}
		}
		free(path);
	}

	/* Are we probing?  If so, we never fail twice. */
	if (probing) {
		trace("Not failing %c due to FAIL_PROBE return\n",
		      info_to_arg[call->type]);
		return call->fail = false;
	}

	/* Don't fail more than once in the same place. */
	dup = failtable_get(&failtable, call);
	if (dup) {
		trace("Not failing %c due to duplicate\n",
		      info_to_arg[call->type]);
		return call->fail = false;
	}

	if (failtest_hook) {
		switch (failtest_hook(&history)) {
		case FAIL_OK:
			break;
		case FAIL_PROBE:
			probing = true;
			break;
		case FAIL_DONT_FAIL:
			trace("Not failing %c due to failhook return\n",
			      info_to_arg[call->type]);
			call->fail = false;
			return false;
		default:
			abort();
		}
	}

	/* Add it to our table of calls. */
	failtable_add(&failtable, call);

	/* We're going to fail in the child. */
	call->fail = true;
	if (pipe(control) != 0 || pipe(output) != 0)
		err(1, "opening pipe");

	/* Move out the way, to high fds. */
	control[0] = move_fd_to_high(control[0]);
	control[1] = move_fd_to_high(control[1]);
	output[0] = move_fd_to_high(output[0]);
	output[1] = move_fd_to_high(output[1]);

	/* Prevent double-printing (in child and parent) */
	fflush(stdout);
	fflush(warnf);
	if (tracef)
		fflush(tracef);
	child = fork();
	if (child == -1)
		err(1, "forking failed");

	if (child == 0) {
		traceindent++;
		if (tracef) {
			struct timerel diff;
			const char *p;
			char *failpath;
			struct failtest_call *c;

			c = tlist_tail(&history, list);
			diff = time_between(time_now(), start);
			failpath = failpath_string();
			p = strrchr(c->file, '/');
			if (p)
				p++;
			else
				p = c->file;
			trace("%u->%u (%u.%02u): %s (%s:%u)\n",
			      getppid(), getpid(),
			      (int)diff.ts.tv_sec, (int)diff.ts.tv_nsec / 10000000,
			      failpath, p, c->line);
			free(failpath);
		}
		/* From here on, we have to clean up! */
		our_history_start = tlist_tail(&history, list);
		close(control[0]);
		close(output[0]);
		/* Don't swallow stderr if we're tracing. */
		if (!tracef) {
			dup2(output[1], STDOUT_FILENO);
			dup2(output[1], STDERR_FILENO);
			if (output[1] != STDOUT_FILENO
			    && output[1] != STDERR_FILENO)
				close(output[1]);
		}
		control_fd = move_fd_to_high(control[1]);

		/* Forget any of our parent's saved files. */
		free_mmapped_files(false);

		/* Now, save any files we need to. */
		save_mmapped_files();

		/* Failed calls can't leak. */
		call->can_leak = false;

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

	/* Only child does probe. */
	probing = false;

	/* We continue onwards without failing. */
	call->fail = false;
	return false;
}

static void cleanup_calloc(struct calloc_call *call, bool restore)
{
	trace("undoing calloc %p\n", call->ret);
	free(call->ret);
}

void *failtest_calloc(size_t nmemb, size_t size,
		      const char *file, unsigned line)
{
	struct failtest_call *p;
	struct calloc_call call;
	call.nmemb = nmemb;
	call.size = size;
	p = add_history(FAILTEST_CALLOC, true, file, line, &call);

	if (should_fail(p)) {
		p->u.calloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		p->u.calloc.ret = calloc(nmemb, size);
		set_cleanup(p, cleanup_calloc, struct calloc_call);
	}
	trace("calloc %zu x %zu %s:%u -> %p\n",
	      nmemb, size, file, line, p->u.calloc.ret);
	errno = p->error;
	return p->u.calloc.ret;
}

static void cleanup_malloc(struct malloc_call *call, bool restore)
{
	trace("undoing malloc %p\n", call->ret);
	free(call->ret);
}

void *failtest_malloc(size_t size, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct malloc_call call;
	call.size = size;

	p = add_history(FAILTEST_MALLOC, true, file, line, &call);
	if (should_fail(p)) {
		p->u.malloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		p->u.malloc.ret = malloc(size);
		set_cleanup(p, cleanup_malloc, struct malloc_call);
	}
	trace("malloc %zu %s:%u -> %p\n",
	      size, file, line, p->u.malloc.ret);
	errno = p->error;
	return p->u.malloc.ret;
}

static void cleanup_realloc(struct realloc_call *call, bool restore)
{
	trace("undoing realloc %p\n", call->ret);
	free(call->ret);
}

/* Walk back and find out if we got this ptr from a previous routine. */
static void fixup_ptr_history(void *ptr, const char *why)
{
	struct failtest_call *i;

	/* Start at end of history, work back. */
	tlist_for_each_rev(&history, i, list) {
		switch (i->type) {
		case FAILTEST_REALLOC:
			if (i->u.realloc.ret == ptr) {
				trace("found realloc %p %s:%u matching %s\n",
				      ptr, i->file, i->line, why);
				i->cleanup = NULL;
				i->can_leak = false;
				return;
			}
			break;
		case FAILTEST_MALLOC:
			if (i->u.malloc.ret == ptr) {
				trace("found malloc %p %s:%u matching %s\n",
				      ptr, i->file, i->line, why);
				i->cleanup = NULL;
				i->can_leak = false;
				return;
			}
			break;
		case FAILTEST_CALLOC:
			if (i->u.calloc.ret == ptr) {
				trace("found calloc %p %s:%u matching %s\n",
				      ptr, i->file, i->line, why);
				i->cleanup = NULL;
				i->can_leak = false;
				return;
			}
			break;
		default:
			break;
		}
	}
	trace("Did not find %p matching %s\n", ptr, why);
}

void *failtest_realloc(void *ptr, size_t size, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct realloc_call call;
	call.size = size;
	p = add_history(FAILTEST_REALLOC, true, file, line, &call);

	/* FIXME: Try one child moving allocation, one not. */
	if (should_fail(p)) {
		p->u.realloc.ret = NULL;
		p->error = ENOMEM;
	} else {
		/* Don't catch this one in the history fixup... */
		p->u.realloc.ret = NULL;
		fixup_ptr_history(ptr, "realloc");
		p->u.realloc.ret = realloc(ptr, size);
		set_cleanup(p, cleanup_realloc, struct realloc_call);
	}
	trace("realloc %p %s:%u -> %p\n",
	      ptr, file, line, p->u.realloc.ret);
	errno = p->error;
	return p->u.realloc.ret;
}

/* FIXME: Record free, so we can terminate fixup_ptr_history correctly.
 * If there's an alloc we don't see, it could get confusing if it matches
 * a previous allocation we did see. */
void failtest_free(void *ptr)
{
	fixup_ptr_history(ptr, "free");
	trace("free %p\n", ptr);
	free(ptr);
}


static struct contents_saved *save_file(const char *pathname)
{
	int fd;
	struct contents_saved *s;

	fd = open(pathname, O_RDONLY);
	if (fd < 0)
		return NULL;

	s = save_contents(pathname, fd, lseek(fd, 0, SEEK_END), 0,
			  "open with O_TRUNC");
	close(fd);
	return s;
}

/* Optimization: don't create a child for an open which *we know*
 * would fail anyway. */
static bool open_would_fail(const char *pathname, int flags)
{
	if ((flags & O_ACCMODE) == O_RDONLY)
		return access(pathname, R_OK) != 0;
	if (!(flags & O_CREAT)) {
		if ((flags & O_ACCMODE) == O_WRONLY)
			return access(pathname, W_OK) != 0;
		if ((flags & O_ACCMODE) == O_RDWR)
			return access(pathname, W_OK) != 0
				|| access(pathname, R_OK) != 0;
	}
	/* FIXME: We could check if it exists, for O_CREAT|O_EXCL */
	return false;
}

static void cleanup_open(struct open_call *call, bool restore)
{
	if (restore && call->saved)
		restore_contents(container_of(call, struct failtest_call,
					      u.open),
				 call->saved, false, "open with O_TRUNC");
	if (!call->closed) {
		trace("Cleaning up open %s by closing fd %i\n",
		      call->pathname, call->ret);
		close(call->ret);
		call->closed = true;
	}
	free(call->saved);
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
	call.always_save = false;
	call.closed = false;
	if (call.flags & O_CREAT) {
		call.mode = va_arg(ap, int);
	}
	va_end(ap);
	p = add_history(FAILTEST_OPEN, true, file, line, &call);
	/* Avoid memory leak! */
	if (p == &unrecorded_call)
		free((char *)call.pathname);

	if (should_fail(p)) {
		/* Don't bother inserting failures that would happen anyway. */
		if (open_would_fail(pathname, call.flags)) {
			trace("Open would have failed anyway: stopping\n");
			failtest_cleanup(true, 0);
		}
		p->u.open.ret = -1;
		/* FIXME: Play with error codes? */
		p->error = EACCES;
	} else {
		/* Save the old version if they're truncating it. */
		if (call.flags & O_TRUNC)
			p->u.open.saved = save_file(pathname);
		else
			p->u.open.saved = NULL;
		p->u.open.ret = open(pathname, call.flags, call.mode);
		if (p->u.open.ret == -1) {
			p->u.open.closed = true;
			p->can_leak = false;
		} else {
			set_cleanup(p, cleanup_open, struct open_call);
		}
	}
	trace("open %s %s:%u -> %i (opener %p)\n",
	      pathname, file, line, p->u.open.ret, &p->u.open);
	errno = p->error;
	return p->u.open.ret;
}

static void cleanup_mmap(struct mmap_call *mmap, bool restore)
{
	trace("cleaning up mmap @%p (opener %p)\n",
	      mmap->ret, mmap->opener);
	if (restore)
		restore_contents(mmap->opener, mmap->saved, false, "mmap");
	free(mmap->saved);
}

void *failtest_mmap(void *addr, size_t length, int prot, int flags,
		    int fd, off_t offset, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct mmap_call call;

	call.addr = addr;
	call.length = length;
	call.prot = prot;
	call.flags = flags;
	call.offset = offset;
	call.fd = fd;
	call.opener = opener_of(fd);

	/* If we don't know what file it was, don't fail. */
	if (!call.opener) {
		if (fd != -1) {
			fwarnx("failtest_mmap: couldn't figure out source for"
			       " fd %i at %s:%u", fd, file, line);
		}
		addr = mmap(addr, length, prot, flags, fd, offset);
		trace("mmap of fd %i -> %p (opener = NULL)\n", fd, addr);
		return addr;
	}

	p = add_history(FAILTEST_MMAP, false, file, line, &call);
	if (should_fail(p)) {
		p->u.mmap.ret = MAP_FAILED;
		p->error = ENOMEM;
	} else {
		p->u.mmap.ret = mmap(addr, length, prot, flags, fd, offset);
		/* Save contents if we're writing to a normal file */
		if (p->u.mmap.ret != MAP_FAILED
		    && (prot & PROT_WRITE)
		    && call.opener->type == FAILTEST_OPEN) {
			const char *fname = call.opener->u.open.pathname;
			p->u.mmap.saved = save_contents(fname, fd, length,
							offset, "being mmapped");
			set_cleanup(p, cleanup_mmap, struct mmap_call);
		}
	}
	trace("mmap of fd %i %s:%u -> %p (opener = %p)\n",
	      fd, file, line, addr, call.opener);
	errno = p->error;
	return p->u.mmap.ret;
}

/* Since OpenBSD can't handle adding args, we use this file and line.
 * This will make all mmaps look the same, reducing coverage. */
void *failtest_mmap_noloc(void *addr, size_t length, int prot, int flags,
			  int fd, off_t offset)
{
	return failtest_mmap(addr, length, prot, flags, fd, offset,
			     __FILE__, __LINE__);
}

static void cleanup_pipe(struct pipe_call *call, bool restore)
{
	trace("cleaning up pipe fd=%i%s,%i%s\n",
	      call->fds[0], call->closed[0] ? "(already closed)" : "",
	      call->fds[1], call->closed[1] ? "(already closed)" : "");
	if (!call->closed[0])
		close(call->fds[0]);
	if (!call->closed[1])
		close(call->fds[1]);
}

int failtest_pipe(int pipefd[2], const char *file, unsigned line)
{
	struct failtest_call *p;
	struct pipe_call call;

	p = add_history(FAILTEST_PIPE, true, file, line, &call);
	if (should_fail(p)) {
		p->u.open.ret = -1;
		/* FIXME: Play with error codes? */
		p->error = EMFILE;
	} else {
		p->u.pipe.ret = pipe(p->u.pipe.fds);
		p->u.pipe.closed[0] = p->u.pipe.closed[1] = false;
		set_cleanup(p, cleanup_pipe, struct pipe_call);
	}

	trace("pipe %s:%u -> %i,%i\n", file, line,
	      p->u.pipe.ret ? -1 : p->u.pipe.fds[0],
	      p->u.pipe.ret ? -1 : p->u.pipe.fds[1]);

	/* This causes valgrind to notice if they use pipefd[] after failure */
	memcpy(pipefd, p->u.pipe.fds, sizeof(p->u.pipe.fds));
	errno = p->error;
	return p->u.pipe.ret;
}

static void cleanup_read(struct read_call *call, bool restore)
{
	if (restore) {
		trace("cleaning up read on fd %i: seeking to %llu\n",
		      call->fd, (long long)call->off);

		/* Read (not readv!) moves file offset! */
		if (lseek(call->fd, call->off, SEEK_SET) != call->off) {
			fwarn("Restoring lseek pointer failed (read)");
		}
	}
}

static ssize_t failtest_add_read(int fd, void *buf, size_t count, off_t off,
				 bool is_pread, const char *file, unsigned line)
{
	struct failtest_call *p;
	struct read_call call;
	call.fd = fd;
	call.buf = buf;
	call.count = count;
	call.off = off;
	p = add_history(FAILTEST_READ, false, file, line, &call);

	/* FIXME: Try partial read returns. */
	if (should_fail(p)) {
		p->u.read.ret = -1;
		p->error = EIO;
	} else {
		if (is_pread)
			p->u.read.ret = pread(fd, buf, count, off);
		else {
			p->u.read.ret = read(fd, buf, count);
			if (p->u.read.ret != -1)
				set_cleanup(p, cleanup_read, struct read_call);
		}
	}
	trace("%sread %s:%u fd %i %zu@%llu -> %zi\n",
	      is_pread ? "p" : "", file, line, fd, count, (long long)off,
	      p->u.read.ret);
	errno = p->error;
	return p->u.read.ret;
}

static void cleanup_write(struct write_call *write, bool restore)
{
	trace("cleaning up write on %s\n", write->opener->u.open.pathname);
	if (restore)
		restore_contents(write->opener, write->saved, !write->is_pwrite,
				 "write");
	free(write->saved);
}

static ssize_t failtest_add_write(int fd, const void *buf,
				  size_t count, off_t off,
				  bool is_pwrite,
				  const char *file, unsigned line)
{
	struct failtest_call *p;
	struct write_call call;

	call.fd = fd;
	call.buf = buf;
	call.count = count;
	call.off = off;
	call.is_pwrite = is_pwrite;
	call.opener = opener_of(fd);
	p = add_history(FAILTEST_WRITE, false, file, line, &call);

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
		bool is_file;
		assert(call.opener == p->u.write.opener);

		if (p->u.write.opener) {
			is_file = (p->u.write.opener->type == FAILTEST_OPEN);
		} else {
			/* We can't unwind it, so at least check same
			 * in parent and child. */
			is_file = false;
		}

		/* FIXME: We assume same write order in parent and child */
		if (!is_file && child_writes_num != 0) {
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

			/* Child wrote it already. */
			trace("write %s:%i on fd %i already done by child\n",
			      file, line, fd);
			p->u.write.ret = count;
			errno = p->error;
			return p->u.write.ret;
		}

		if (is_file) {
			p->u.write.saved = save_contents(call.opener->u.open.pathname,
							 fd, count, off,
							 "being overwritten");
			set_cleanup(p, cleanup_write, struct write_call);
		}

		/* Though off is current seek ptr for write case, we need to
		 * move it.  write() does that for us. */
		if (p->u.write.is_pwrite)
			p->u.write.ret = pwrite(fd, buf, count, off);
		else
			p->u.write.ret = write(fd, buf, count);
	}
	trace("%swrite %s:%i %zu@%llu on fd %i -> %zi\n",
	      p->u.write.is_pwrite ? "p" : "",
	      file, line, count, (long long)off, fd, p->u.write.ret);
	errno = p->error;
	return p->u.write.ret;
}

ssize_t failtest_pwrite(int fd, const void *buf, size_t count, off_t offset,
			const char *file, unsigned line)
{
	return failtest_add_write(fd, buf, count, offset, true, file, line);
}

ssize_t failtest_write(int fd, const void *buf, size_t count,
		       const char *file, unsigned line)
{
	return failtest_add_write(fd, buf, count, lseek(fd, 0, SEEK_CUR), false,
				  file, line);
}

ssize_t failtest_pread(int fd, void *buf, size_t count, off_t off,
		       const char *file, unsigned line)
{
	return failtest_add_read(fd, buf, count, off, true, file, line);
}

ssize_t failtest_read(int fd, void *buf, size_t count,
		      const char *file, unsigned line)
{
	return failtest_add_read(fd, buf, count, lseek(fd, 0, SEEK_CUR), false,
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
			trace("splitting lock on fd %i from %llu-%llu"
			      " to %llu-%llu\n",
			      fd, (long long)l->start, (long long)l->end,
			      (long long)l->start, (long long)start - 1);
			l->end = start - 1;
			locks = add_lock(locks,
					 fd, new_start, new_end, l->type);
			l = &locks[i];
		} else if (start <= l->start && end >= l->end) {
			/* Total overlap: eliminate entry. */
			trace("erasing lock on fd %i %llu-%llu\n",
			      fd, (long long)l->start, (long long)l->end);
			l->end = 0;
			l->start = 1;
		} else if (end >= l->start && end < l->end) {
			trace("trimming lock on fd %i from %llu-%llu"
			      " to %llu-%llu\n",
			      fd, (long long)l->start, (long long)l->end,
			      (long long)end + 1, (long long)l->end);
			/* Start overlap: trim entry. */
			l->start = end + 1;
		} else if (start > l->start && start <= l->end) {
			trace("trimming lock on fd %i from %llu-%llu"
			      " to %llu-%llu\n",
			      fd, (long long)l->start, (long long)l->end,
			      (long long)l->start, (long long)start - 1);
			/* End overlap: trim entry. */
			l->end = start-1;
		}
		/* Nothing left?  Remove it. */
		if (l->end < l->start) {
			trace("forgetting lock on fd %i\n", fd);
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
		trace("new lock on fd %i %llu-%llu\n",
		      fd, (long long)l->start, (long long)l->end);
	}
	return locks;
}

/* We trap this so we can record it: we don't fail it. */
int failtest_close(int fd, const char *file, unsigned line)
{
	struct close_call call;
	struct failtest_call *p, *opener;

	/* Do this before we add ourselves to history! */
	opener = opener_of(fd);

	call.fd = fd;
	p = add_history(FAILTEST_CLOSE, false, file, line, &call);
	p->fail = false;

	/* Consume close from failpath (shouldn't tell us to fail). */
	if (following_path()) {
		if (follow_path(p))
			abort();
	}

	trace("close on fd %i\n", fd);
	if (fd < 0)
		return close(fd);

	/* Mark opener as not leaking, remove its cleanup function. */
	if (opener) {
		trace("close on fd %i found opener %p\n", fd, opener);
		if (opener->type == FAILTEST_PIPE) {
			/* From a pipe? */
			if (opener->u.pipe.fds[0] == fd) {
				assert(!opener->u.pipe.closed[0]);
				opener->u.pipe.closed[0] = true;
			} else if (opener->u.pipe.fds[1] == fd) {
				assert(!opener->u.pipe.closed[1]);
				opener->u.pipe.closed[1] = true;
			} else
				abort();
			opener->can_leak = (!opener->u.pipe.closed[0]
					    || !opener->u.pipe.closed[1]);
		} else if (opener->type == FAILTEST_OPEN) {
			opener->u.open.closed = true;
			opener->can_leak = false;
		} else
			abort();
	}

	/* Restore offset now, in case parent shared (can't do after close!). */
	if (control_fd != -1) {
		struct failtest_call *i;

		tlist_for_each_rev(&history, i, list) {
			if (i == our_history_start)
				break;
			if (i == opener)
				break;
			if (i->type == FAILTEST_LSEEK && i->u.lseek.fd == fd) {
				trace("close on fd %i undoes lseek\n", fd);
				/* This seeks back. */
				i->cleanup(&i->u, true);
				i->cleanup = NULL;
			} else if (i->type == FAILTEST_WRITE
				   && i->u.write.fd == fd
				   && !i->u.write.is_pwrite) {
				trace("close on fd %i undoes write"
				      " offset change\n", fd);
				/* Write (not pwrite!) moves file offset! */
				if (lseek(fd, i->u.write.off, SEEK_SET)
				    != i->u.write.off) {
					fwarn("Restoring lseek pointer failed (write)");
				}
			} else if (i->type == FAILTEST_READ
				   && i->u.read.fd == fd) {
				/* preads don't *have* cleanups */
				if (i->cleanup) {
					trace("close on fd %i undoes read"
					      " offset change\n", fd);
					/* This seeks back. */
					i->cleanup(&i->u, true);
					i->cleanup = NULL;
				}
			}
		}
	}

	/* Close unlocks everything. */
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
		trace("fcntl on fd %i F_SETFL/F_SETFD\n", fd);
		return fcntl(fd, cmd, call.arg.l);
	case F_GETFD:
	case F_GETFL:
		trace("fcntl on fd %i F_GETFL/F_GETFD\n", fd);
		return fcntl(fd, cmd);
	case F_GETLK:
		trace("fcntl on fd %i F_GETLK\n", fd);
		get_locks();
		va_start(ap, cmd);
		call.arg.fl = *va_arg(ap, struct flock *);
		va_end(ap);
		return fcntl(fd, cmd, &call.arg.fl);
	case F_SETLK:
	case F_SETLKW:
		trace("fcntl on fd %i F_SETLK%s\n",
		      fd, cmd == F_SETLKW ? "W" : "");
		va_start(ap, cmd);
		call.arg.fl = *va_arg(ap, struct flock *);
		va_end(ap);
		break;
	default:
		/* This means you need to implement it here. */
		err(1, "failtest: unknown fcntl %u", cmd);
	}

	p = add_history(FAILTEST_FCNTL, false, file, line, &call);

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
	trace("fcntl on fd %i -> %i\n", fd, p->u.fcntl.ret);
	errno = p->error;
	return p->u.fcntl.ret;
}

static void cleanup_lseek(struct lseek_call *call, bool restore)
{
	if (restore) {
		trace("cleaning up lseek on fd %i -> %llu\n",
		      call->fd, (long long)call->old_off);
		if (lseek(call->fd, call->old_off, SEEK_SET) != call->old_off)
			fwarn("Restoring lseek pointer failed");
	}
}

/* We trap this so we can undo it: we don't fail it. */
off_t failtest_lseek(int fd, off_t offset, int whence, const char *file,
		     unsigned int line)
{
	struct failtest_call *p;
	struct lseek_call call;
	call.fd = fd;
	call.offset = offset;
	call.whence = whence;
	call.old_off = lseek(fd, 0, SEEK_CUR);

	p = add_history(FAILTEST_LSEEK, false, file, line, &call);
	p->fail = false;

	/* Consume lseek from failpath. */
	if (failpath)
		if (should_fail(p))
			abort();

	p->u.lseek.ret = lseek(fd, offset, whence);

	if (p->u.lseek.ret != (off_t)-1)
		set_cleanup(p, cleanup_lseek, struct lseek_call);

	trace("lseek %s:%u on fd %i from %llu to %llu%s\n",
	      file, line, fd, (long long)call.old_off, (long long)offset,
	      whence == SEEK_CUR ? " (from current off)" :
	      whence == SEEK_END ? " (from end)" :
	      whence == SEEK_SET ? "" : " (invalid whence)");
	return p->u.lseek.ret;
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

	warnf = fdopen(move_fd_to_high(dup(STDERR_FILENO)), "w");
	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--failpath=", strlen("--failpath="))) {
			failpath = argv[i] + strlen("--failpath=");
		} else if (strcmp(argv[i], "--trace") == 0) {
			tracef = warnf;
			failtest_timeout_ms = -1;
		} else if (!strncmp(argv[i], "--debugpath=",
				    strlen("--debugpath="))) {
			debugpath = argv[i] + strlen("--debugpath=");
		}
	}
	failtable_init(&failtable);
	start = time_now();
}

bool failtest_has_failed(void)
{
	return control_fd != -1;
}

void failtest_exit(int status)
{
	trace("failtest_exit with status %i\n", status);
	if (failtest_exit_check) {
		if (!failtest_exit_check(&history))
			child_fail(NULL, 0, "failtest_exit_check failed\n");
	}

	failtest_cleanup(false, status);
}
