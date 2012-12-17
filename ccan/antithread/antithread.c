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
#include <ccan/tal/tal.h>
#include <ccan/tal/str/str.h>
#include <ccan/antithread/alloc/alloc.h>
#include <ccan/list/list.h>

/* FIXME: Valgrind support should be possible for some cases.  Tricky
 * case is where another process allocates for you, but at worst we
 * could reset what is valid and what isn't on every entry into the
 * library or something. */

/* FIXME: If tal supported multiple set_share, it would be easy to have
 * multiple pools.   Search for single_pool to see where we need this. */
static struct at_pool *single_pool;
static struct at_parent *single_parent;

/* This sits in the pool itself, but is only written by the parent.
 * It has to be inside the tal_shared region so allocations using it as
 * context are inside the pool. */
struct at_pool {
	char *map;
	size_t mapsize;
	int fd;
};

struct at_child {
	pid_t pid;
	int rfd, wfd;
};

struct at_parent {
	struct at_pool *pool;
	int rfd, wfd;
};

/* FIXME: Better locking through futexes. */
#define ALLOC_LOCK_OFF 1

static void lock(int fd, unsigned long off)
{
	struct flock fl;

	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 1;

	while (fcntl(fd, F_SETLKW, &fl) < 0) {
		if (errno != EINTR)
			err(1, "Failure locking antithread file off %llu",
			    (long long)off);
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

#define ALIGNMENT (sizeof(void *)*2)

static void *pool_alloc(size_t size, struct at_pool *p)
{
	void *ret;

	/* FIXME: Alignment. */
	lock(p->fd, ALLOC_LOCK_OFF);
	ret = alloc_get(p->map, p->mapsize, size, ALIGNMENT);
	unlock(p->fd, ALLOC_LOCK_OFF);

	return ret;
}

static void *pool_resize(void *ptr, size_t size, struct at_pool *p)
{
	char *map;
	size_t mapsize;
	void *new;

	map = p->map;
	mapsize = p->mapsize;

	/* FIXME: resize in alloc? */
	if (size <= alloc_size(map, mapsize, ptr))
		return ptr;

	lock(p->fd, ALLOC_LOCK_OFF);
	new = alloc_get(map, mapsize, size, ALIGNMENT);
	if (new) {
		memcpy(new, ptr, alloc_size(map, mapsize, ptr));
		alloc_free(map, mapsize, ptr);
	}
	unlock(p->fd, ALLOC_LOCK_OFF);
	return new;
}

static void pool_parent_free(void *ptr, struct at_pool *p)
{
	/* We trap the free of the pool object here: we can't do it in
	 * the destructor because children are yet to be destroyed. */
	if (ptr == p) {
		close(p->fd);
		munmap(p->map, p->mapsize);
		single_pool = NULL;
	} else {
		lock(p->fd, ALLOC_LOCK_OFF);
		alloc_free(p->map, p->mapsize, ptr);
		unlock(p->fd, ALLOC_LOCK_OFF);
	}
}

static void pool_child_free(void *ptr, struct at_pool *p)
{
	/* We trap the free of the pool object here: we can't do it in
	 * the destructor because children are yet to be destroyed. */
	if (ptr == single_parent) {
		close(p->fd);
		munmap(p->map, p->mapsize);
		single_pool = NULL;
	} else {
		lock(p->fd, ALLOC_LOCK_OFF);
		alloc_free(p->map, p->mapsize, ptr);
		unlock(p->fd, ALLOC_LOCK_OFF);
	}
}


static void pool_lock(const tal_t *obj, struct at_pool *p)
{
	lock(p->fd, (char *)obj - p->map);
}

static void pool_unlock(const void *obj, struct at_pool *p)
{
	unlock(p->fd, (char *)obj - p->map);
}

/* We add 16MB to size.  This compensates for address randomization. */
#define PADDING (16 * 1024 * 1024)

/* Create a new sharable pool. */
struct at_pool *at_new_pool(size_t size)
{
	int fd;
	char *mem;
	FILE *f;
	struct at_pool tmp_pool, *pool;

	/* FIXME: tal only handles a single pool for now */
	assert(!single_pool);

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

	/* First map gets a nice big area. */
	mem = mmap(NULL, size+PADDING, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mem == MAP_FAILED)
		goto fail_close;

	/* Then we remap into the middle of it. */
	munmap(mem, size+PADDING);
	mem = mmap(mem + PADDING/2, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd,
		   0);
	if (mem == MAP_FAILED)
		goto fail_close;

	alloc_init(mem, size);

	/* Create temporary pool struct for first allocation. */
	tmp_pool.fd = fd;
	tmp_pool.map = mem;
	tmp_pool.mapsize = size;

	pool = tal_shared_init(sizeof(*pool), pool_alloc, &tmp_pool);
	*pool = tmp_pool;

	tal_set_shared(mem, size,
		       pool_alloc, pool_resize, pool_parent_free,
		       pool_lock, pool_unlock, pool);
	return single_pool = pool;

fail_close:
	close_noerr(fd);
	return NULL;
}

static void destroy_child(struct at_child *child)
{
	close(child->rfd);
	close(child->wfd);

	/* If it is already a zombie, this is harmless. */
	kill(child->pid, SIGTERM);

	/* FIXME: Should we do SIGKILL if process doesn't exit soon? */
	if (waitpid(child->pid, NULL, 0) != child->pid)
		err(1, "Waiting for at_child %p (pid %u)", child, child->pid);
}

/* Sets up thread and forks it.  Both parent and return NULL on error. */
static struct at_child *fork_thread(struct at_pool *pool,
				    struct at_parent **parent)
{
	int p2c[2], c2p[2];
	pid_t pid;

	*parent = NULL;

	if (pipe(p2c) != 0)
		goto out;

	if (pipe(c2p) != 0)
		goto close_p2c;

	pid = fork();
	if (pid == -1)
		goto close_c2p;

	if (pid == 0) {
		/* Child */
		*parent = tal(pool, struct at_parent);
		(*parent)->pool = pool;
		(*parent)->rfd = p2c[0];
		(*parent)->wfd = c2p[1];
		close(c2p[0]);
		close(p2c[1]);
		return NULL;
	} else {
		/* Parent. */
		struct at_child *child = tal(pool, struct at_child);
		close(c2p[1]);
		close(p2c[0]);
		child->rfd = c2p[0];
		child->wfd = p2c[1];
		child->pid = pid;
		tal_add_destructor(child, destroy_child);
		return child;
	}

close_c2p:
	close_noerr(c2p[0]);
	close_noerr(c2p[1]);
close_p2c:
	close_noerr(p2c[0]);
	close_noerr(p2c[1]);
out:
	return NULL;
}

/* Creating an antithread via fork() */
struct at_child *_at_run(struct at_pool *pool,
			void *(*fn)(struct at_parent *, void *),
			void *obj)
{
	struct at_child *child;
	struct at_parent *parent;

	child = fork_thread(pool, &parent);
	if (!child && !parent)
		return NULL;

	if (parent) {
		/* Child */
		tal_set_shared(pool->map, pool->mapsize,
			       pool_alloc, pool_resize, pool_child_free,
			       pool_lock, pool_unlock, pool);
		at_tell_parent(parent, fn(parent, obj));
		exit(0);
	}
	/* Parent */
	return child;
}

static unsigned int num_args(char *const argv[])
{
	unsigned int i;

	for (i = 0; argv[i]; i++);
	return i;
}

/* Fork and execvp, with added arguments for child to grab. */
struct at_child *at_spawn(struct at_pool *pool, void *arg, char *cmdline[])
{
	struct at_child *child;
	struct at_parent *parent;
	int err;

	child = fork_thread(pool, &parent);
	if (!child && !parent)
		return NULL;

	if (parent) {
		/* child */
		char extra_arg[sizeof("AT:%p/%zu/%u/%p/%p")
			       + STR_MAX_CHARS(pool->map)
			       + STR_MAX_CHARS(pool->mapsize)
			       + STR_MAX_CHARS(pool->fd)
			       + STR_MAX_CHARS(parent)
			       + STR_MAX_CHARS(arg)];
		char *argv[num_args(cmdline) + 2];
		argv[0] = cmdline[0];
		sprintf(extra_arg, "AT:%p/%zu/%u/%p/%p",
			pool->map, pool->mapsize, pool->fd, parent, arg);
		argv[1] = extra_arg;
		/* Copy including NULL terminator. */
		memcpy(&argv[2], &cmdline[1], num_args(cmdline)*sizeof(char *));
		execvp(argv[0], argv);

		err = errno;
		/* Bogus if prevents Ubuntu warn_unused_result bogosity. */
		if (write(parent->wfd, &err, sizeof(err)));
		exit(1);
	}

	/* Child should always write an error code (or 0). */
	if (read(child->rfd, &err, sizeof(err)) != sizeof(err)) {
		errno = ECHILD;
		tal_free(child);
		return NULL;
	}
	if (err != 0) {
		errno = err;
		tal_free(child);
		return NULL;
	}
	return child;
}

/* The fd to poll on */
int at_child_rfd(struct at_child *child)
{
	return child->rfd;
}

/* What's the antithread saying?  Blocks if fd not ready. */
void *at_read_child(const struct at_child *child)
{
	void *ret;

	switch (read(child->rfd, &ret, sizeof(ret))) {
	case -1:
		err(1, "Reading from at_child %p (pid %u)", child, child->pid);
	case 0:
		/* Thread died. */
		return NULL;
	case sizeof(ret):
		return ret;
	default:
		/* Should never happen. */
		err(1, "Short read from at_child %p (pid %u)",
		    child, child->pid);
	}
}

/* Say something to a child. */
bool at_tell_child(const struct at_child *at, const void *ptr)
{
	return write(at->wfd, &ptr, sizeof(ptr)) == sizeof(ptr);
}

/* For child to grab arguments from command line (removes them) */
struct at_parent *at_get_parent(int *argc, char *argv[], void **arg)
{
	struct at_parent *parent;
	void *map, *m;
	int err, fd;
	size_t size;

	if (!argv[1]) {
		errno = EINVAL;
		return NULL;
	}

	/* If they don't care, use dummy value. */
	if (arg == NULL)
		arg = &m;

	if (sscanf(argv[1], "AT:%p/%zu/%u/%p/%p",
		   &map, &size, &fd, &parent, arg) != 5) {
		errno = EINVAL;
		return NULL;
	}

	/* FIXME: To try to adjust for address space randomization, we
	 * could re-exec a few times. */
	m = mmap(map, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (m != map) {
		fprintf(stderr, "Mapping %zu bytes @%p gave %p\n",
			size, map, m);
		errno = ENOMEM;
		return NULL;
	}

	single_parent = parent;
	single_pool = parent->pool;
	tal_set_shared(parent->pool->map, parent->pool->mapsize,
		       pool_alloc, pool_resize, pool_child_free,
		       pool_lock, pool_unlock, parent->pool);

	/* Tell parent we're good. */
	err = 0;
	if (write(parent->wfd, &err, sizeof(err)) != sizeof(err)) {
		munmap(map, size);
		errno = EBADF;
		return NULL;
	}

	/* Delete AT arg. */
	memmove(&argv[1], &argv[2], --(*argc));

	/* FIXME: Somehow unmap parent if its freed... */
	return parent;
}

/* Say something to our parent (async). */
bool at_tell_parent(struct at_parent *parent, const void *ptr)
{
	return write(parent->wfd, &ptr, sizeof(ptr)) == sizeof(ptr);
}

/* What's the parent saying?  Blocks if fd not ready. */
void *at_read_parent(struct at_parent *parent)
{
	void *ret;

	switch (read(parent->rfd, &ret, sizeof(ret))) {
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
int at_parent_rfd(struct at_parent *parent)
{
	if (parent->rfd == -1)
		errx(1, "This process is not an antithread of this pool");

	return parent->rfd;
}

/* FIXME: Futexme. */
void at_lock(const void *obj)
{
	struct at_pool *p = single_pool;

	lock(p->fd, (char *)obj - (char *)p->map);
}

void at_unlock(const void *obj)
{
	struct at_pool *p = single_pool;

	unlock(p->fd, (char *)obj - (char *)p->map);
}

bool at_check_pool(struct at_pool *pool, const char *abortstr)
{
	struct tal_t *i;

	/* First look for non-pool objects in the pool. */
	for (i = tal_first(pool); i; i = tal_next(pool, i)) {
		if ((char *)i < (char *)pool->map
		    || (char *)i >= (char *)pool->map + pool->mapsize) {
			if (!abortstr)
				return false;
			fprintf(stderr, "%s: child %p outside pool!\n",
				abortstr, i);
			abort();
		}
	}

	/* Now check tal tree. */
	if (!tal_check(pool, abortstr))
		return false;

	return true;
}
