/* Licensed under GPLv3+ - see LICENSE file for details */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "antithread.h"
#include <ccan/err/err.h>
#include <ccan/noerr/noerr.h>
#include <ccan/talloc/talloc.h>
#include <ccan/read_write_all/read_write_all.h>
#include <ccan/antithread/alloc/alloc.h>
#include <ccan/list/list.h>

/* FIXME: Valgrind support should be possible for some cases.  Tricky
 * case is where another process allocates for you, but at worst we
 * could reset what is valid and what isn't on every entry into the
 * library or something. */

static LIST_HEAD(pools);

/* Talloc destroys parents before children (damn Tridge's failing destructors!)
 * so we need the first child (ie. last-destroyed) to actually clean up. */
struct at_pool_contents {
	struct list_node list;
	void *pool;
	unsigned long poolsize;
	int fd;
	int parent_rfd, parent_wfd;
	struct at_pool *atp;
};

struct at_pool {
	struct at_pool_contents *p;
	const void *ctx;
};

struct athread {
	pid_t pid;
	int rfd, wfd;
};

/* FIXME: Better locking through futexes. */
static void lock(int fd, unsigned long off)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 1;

	while (fcntl(fd, F_SETLKW, &fl) < 0) {
		if (errno != EINTR)
			err(1, "Failure locking antithread file");
	}
}

static void unlock(int fd, unsigned long off)
{
	struct flock fl;
	int serrno = errno;

	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 1;

	fcntl(fd, F_SETLK, &fl);
	errno = serrno;
}

/* This pointer is in a pool.  Find which one. */
static struct at_pool_contents *find_pool(const void *ptr)
{
	struct at_pool_contents *p;

	list_for_each(&pools, p, list) {
		/* Special case for initial allocation: ptr *is* pool */
		if (ptr == p->atp)
			return p;

		if ((char *)ptr >= (char *)p->pool
		    && (char *)ptr < (char *)p->pool + p->poolsize)
			return p;
	}
	abort();
}

static int destroy_pool(struct at_pool_contents *p)
{
	list_del(&p->list);
	munmap(p->pool, p->poolsize);
	close(p->fd);
	close(p->parent_rfd);
	close(p->parent_wfd);
	return 0;
}

static void *at_realloc(const void *parent, void *ptr, size_t size)
{
	struct at_pool_contents *p = find_pool(parent);
	/* FIXME: realloc in ccan/alloc? */
	void *new;

	if (size == 0) {
		alloc_free(p->pool, p->poolsize, ptr);
		new = NULL;
	} else if (ptr == NULL) {
		/* FIXME: Alignment */
		new = alloc_get(p->pool, p->poolsize, size, 16);
	} else {
		if (size <= alloc_size(p->pool, p->poolsize, ptr))
			new = ptr;
		else {
			new = alloc_get(p->pool, p->poolsize, size, 16);
			if (new) {
				memcpy(new, ptr,
				       alloc_size(p->pool, p->poolsize, ptr));
				alloc_free(p->pool, p->poolsize, ptr);
			}
		}
	}

	return new;
}

static struct at_pool_contents *locked;
static void talloc_lock(const void *ptr)
{
	struct at_pool_contents *p = find_pool(ptr);

	lock(p->fd, 0);
	assert(!locked);
	locked = p;
}

static void talloc_unlock(void)
{
	struct at_pool_contents *p = locked;

	locked = NULL;
	unlock(p->fd, 0);
}

/* We add 16MB to size.  This compensates for address randomization. */
#define PADDING (16 * 1024 * 1024)

/* Create a new sharable pool. */
struct at_pool *at_pool(unsigned long size)
{
	int fd;
	struct at_pool *atp;
	struct at_pool_contents *p;
	FILE *f;

	/* FIXME: How much should we actually add for overhead?. */
	size += 32 * getpagesize();

	/* Round up to whole pages. */
	size = (size + getpagesize()-1) & ~(getpagesize()-1);

	f = tmpfile();
	if (!f)
		return NULL;

	fd = dup(fileno(f));
	fclose_noerr(f);

	if (fd < 0)
		return NULL;

	if (ftruncate(fd, size + PADDING) != 0)
		goto fail_close;

	atp = talloc(NULL, struct at_pool);
	if (!atp)
		goto fail_close;

	atp->p = p = talloc(NULL, struct at_pool_contents);
	if (!p)
		goto fail_free;

	/* First map gets a nice big area. */
	p->pool = mmap(NULL, size+PADDING, PROT_READ|PROT_WRITE, MAP_SHARED, fd,
		       0);
	if (p->pool == MAP_FAILED)
		goto fail_free;

	/* Then we remap into the middle of it. */
	munmap(p->pool, size+PADDING);
	p->pool = mmap((char *)p->pool + PADDING/2, size, PROT_READ|PROT_WRITE,
		       MAP_SHARED, fd, 0);
	if (p->pool == MAP_FAILED)
		goto fail_free;

	p->fd = fd;
	p->poolsize = size;
	p->parent_rfd = p->parent_wfd = -1;
	p->atp = atp;
	alloc_init(p->pool, p->poolsize);
	list_add(&pools, &p->list);
	talloc_set_destructor(p, destroy_pool);

	atp->ctx = talloc_add_external(atp,
				       at_realloc, talloc_lock, talloc_unlock);
	if (!atp->ctx)
		goto fail_free;
	return atp;

fail_free:
	talloc_free(atp);
fail_close:
	close_noerr(fd);
	return NULL;
}

/* Talloc off this to allocate from within the pool. */
const void *at_pool_ctx(struct at_pool *atp)
{
	return atp->ctx;
}

static int cant_destroy_self(struct athread *at)
{
	/* Perhaps this means we want to detach, but it doesn't really
	 * make sense. */
	abort();
	return 0;
}

static int destroy_at(struct athread *at)
{
	/* If it is already a zombie, this is harmless. */
	kill(at->pid, SIGTERM);

	close(at->rfd);
	close(at->wfd);

	/* FIXME: Should we do SIGKILL if process doesn't exit soon? */
	if (waitpid(at->pid, NULL, 0) != at->pid)
		err(1, "Waiting for athread %p (pid %u)", at, at->pid);

	return 0;
}

/* Sets up thread and forks it.  NULL on error. */
static struct athread *fork_thread(struct at_pool *atp)
{
	int p2c[2], c2p[2];
	struct athread *at;
	struct at_pool_contents *pool = atp->p;

	/* You can't already be a child of this pool. */
	if (pool->parent_rfd != -1)
		errx(1, "Can't create antithread on this pool: we're one");

	/* We don't want this allocated *in* the pool. */
	at = talloc_steal(atp, talloc(NULL, struct athread));

	if (pipe(p2c) != 0)
		goto free;

	if (pipe(c2p) != 0)
		goto close_p2c;

	at->pid = fork();
	if (at->pid == -1)
		goto close_c2p;

	if (at->pid == 0) {
		/* Child */
		close(c2p[0]);
		close(p2c[1]);
		pool->parent_rfd = p2c[0];
		pool->parent_wfd = c2p[1];
		talloc_set_destructor(at, cant_destroy_self);
	} else {
		/* Parent */
		close(c2p[1]);
		close(p2c[0]);
		at->rfd = c2p[0];
		at->wfd = p2c[1];
		talloc_set_destructor(at, destroy_at);
	}

	return at;
close_c2p:
	close_noerr(c2p[0]);
	close_noerr(c2p[1]);
close_p2c:
	close_noerr(p2c[0]);
	close_noerr(p2c[1]);
free:
	talloc_free(at);
	return NULL;
}

/* Creating an antithread via fork() */
struct athread *_at_run(struct at_pool *atp,
			void *(*fn)(struct at_pool *, void *),
			void *obj)
{
	struct athread *at;

	at = fork_thread(atp);
	if (!at)
		return NULL;

	if (at->pid == 0) {
		/* Child */
		at_tell_parent(atp, fn(atp, obj));
		exit(0);
	}
	/* Parent */
	return at;
}

static unsigned int num_args(char *const argv[])
{
	unsigned int i;

	for (i = 0; argv[i]; i++);
	return i;
}

/* Fork and execvp, with added arguments for child to grab. */
struct athread *at_spawn(struct at_pool *atp, void *arg, char *cmdline[])
{
	struct athread *at;
	int err;

	at = fork_thread(atp);
	if (!at)
		return NULL;

	if (at->pid == 0) {
		/* child */
		char *argv[num_args(cmdline) + 2];
		argv[0] = cmdline[0];
		argv[1] = talloc_asprintf(NULL, "AT:%p/%lu/%i/%i/%i/%p",
					  atp->p->pool, atp->p->poolsize,
					  atp->p->fd, atp->p->parent_rfd,
					  atp->p->parent_wfd, arg);
		/* Copy including NULL terminator. */
		memcpy(&argv[2], &cmdline[1], num_args(cmdline)*sizeof(char *));
		execvp(argv[0], argv);

		err = errno;
		write_all(atp->p->parent_wfd, &err, sizeof(err));
		exit(1);
	}

	/* Child should always write an error code (or 0). */
	if (read(at->rfd, &err, sizeof(err)) != sizeof(err)) {
		errno = ECHILD;
		talloc_free(at);
		return NULL;
	}
	if (err != 0) {
		errno = err;
		talloc_free(at);
		return NULL;
	}
	return at;
}

/* The fd to poll on */
int at_fd(struct athread *at)
{
	return at->rfd;
}

/* What's the antithread saying?  Blocks if fd not ready. */
void *at_read(struct athread *at)
{
	void *ret;

	switch (read(at->rfd, &ret, sizeof(ret))) {
	case -1:
		err(1, "Reading from athread %p (pid %u)", at, at->pid);
	case 0:
		/* Thread died. */
		return NULL;
	case sizeof(ret):
		return ret;
	default:
		/* Should never happen. */
		err(1, "Short read from athread %p (pid %u)", at, at->pid);
	}
}

/* Say something to a child. */
void at_tell(struct athread *at, const void *status)
{
	if (write(at->wfd, &status, sizeof(status)) != sizeof(status))
		err(1, "Failure writing to athread %p (pid %u)", at, at->pid);
}

/* For child to grab arguments from command line (removes them) */
struct at_pool *at_get_pool(int *argc, char *argv[], void **arg)
{
	struct at_pool *atp = talloc(NULL, struct at_pool);
	struct at_pool_contents *p;
	void *map;
	int err;

	if (!argv[1]) {
		errno = EINVAL;
		goto fail;
	}

	/* If they don't care, use dummy value. */
	if (arg == NULL)
		arg = &map;

	p = atp->p = talloc(atp, struct at_pool_contents);

	if (sscanf(argv[1], "AT:%p/%lu/%i/%i/%i/%p",
		   &p->pool, &p->poolsize, &p->fd,
		   &p->parent_rfd, &p->parent_wfd, arg) != 6) {
		errno = EINVAL;
		goto fail;
	}

	/* FIXME: To try to adjust for address space randomization, we
	 * could re-exec a few times. */
	map = mmap(p->pool, p->poolsize, PROT_READ|PROT_WRITE, MAP_SHARED,
		   p->fd, 0);
	if (map != p->pool) {
		fprintf(stderr, "Mapping %lu bytes @%p gave %p\n",
			p->poolsize, p->pool, map);
		errno = ENOMEM;
		goto fail;
	}

	list_add(&pools, &p->list);
	talloc_set_destructor(p, destroy_pool);
	p->atp = atp;

	atp->ctx = talloc_add_external(atp,
				       at_realloc, talloc_lock, talloc_unlock);
	if (!atp->ctx)
		goto fail;

	/* Tell parent we're good. */
	err = 0;
	if (write(p->parent_wfd, &err, sizeof(err)) != sizeof(err)) {
		errno = EBADF;
		goto fail;
	}

	/* Delete AT arg. */
	memmove(&argv[1], &argv[2], --(*argc));

	return atp;

fail:
	talloc_free(atp);
	return NULL;
}

/* Say something to our parent (async). */
void at_tell_parent(struct at_pool *atp, const void *status)
{
	if (atp->p->parent_wfd == -1)
		errx(1, "This process is not an antithread of this pool");

	if (write(atp->p->parent_wfd, &status, sizeof(status))!=sizeof(status))
		err(1, "Failure writing to parent");
}

/* What's the parent saying?  Blocks if fd not ready. */
void *at_read_parent(struct at_pool *atp)
{
	void *ret;

	if (atp->p->parent_rfd == -1)
		errx(1, "This process is not an antithread of this pool");

	switch (read(atp->p->parent_rfd, &ret, sizeof(ret))) {
	case -1:
		err(1, "Reading from parent");
	case 0:
		/* Parent died. */
		return NULL;
	case sizeof(ret):
		return ret;
	default:
		/* Should never happen. */
		err(1, "Short read from parent");
	}
}

/* The fd to poll on */
int at_parent_fd(struct at_pool *atp)
{
	if (atp->p->parent_rfd == -1)
		errx(1, "This process is not an antithread of this pool");

	return atp->p->parent_rfd;
}

/* FIXME: Futexme. */
void at_lock(void *obj)
{
	struct at_pool *atp = talloc_find_parent_bytype(obj, struct at_pool);
#if 0
	unsigned int *l;

	/* This isn't required yet, but ensures it's a talloc ptr */
	l = talloc_lock_ptr(obj);
#endif

	lock(atp->p->fd, (char *)obj - (char *)atp->p->pool);

#if 0
	if (*l)
		errx(1, "Object %p was already locked (something died?)", obj);
	*l = 1;
#endif
}

void at_unlock(void *obj)
{
	struct at_pool *atp = talloc_find_parent_bytype(obj, struct at_pool);
#if 0
	unsigned int *l;

	l = talloc_lock_ptr(obj);
	if (!*l)
		errx(1, "Object %p was already unlocked", obj);
	*l = 0;
#endif
	unlock(atp->p->fd, (char *)obj - (char *)atp->p->pool);
}

void at_lock_all(struct at_pool *atp)
{
	lock(atp->p->fd, 0);
}
	
void at_unlock_all(struct at_pool *atp)
{
	unlock(atp->p->fd, 0);
}
