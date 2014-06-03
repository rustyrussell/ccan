/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include "io.h"
#include "backend.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

void *io_loop_return;

struct io_alloc io_alloc = {
	malloc, realloc, free
};

#ifdef DEBUG
/* Set to skip the next plan. */
bool io_plan_nodebug;
/* The current connection to apply plan to. */
struct io_conn *current;
/* User-defined function to select which connection(s) to debug. */
bool (*io_debug_conn)(struct io_conn *conn);

struct io_plan io_debug(struct io_plan plan)
{
	struct io_conn *ready = NULL;

	if (io_plan_nodebug) {
		io_plan_nodebug = false;
		return plan;
	}

	if (!current || !doing_debug_on(current))
		return plan;

	current->plan = plan;
	backend_plan_changed(current);

	/* Call back into the loop immediately. */
	io_loop_return = do_io_loop(&ready);

	if (ready) {
		set_current(ready);
		if (!ready->plan.next) {
			/* Call finish function immediately. */
			if (ready->finish) {
				errno = ready->plan.u1.s;
				ready->finish(ready, ready->finish_arg);
				ready->finish = NULL;
			}
			backend_del_conn(ready);
		} else {
			/* Calls back in itself, via io_debug_io(). */
			if (ready->plan.io(ready->fd.fd, &ready->plan) != 2)
				abort();
		}
		set_current(NULL);
	}

	/* Return a do-nothing plan, so backend_plan_changed in
	 * io_ready doesn't do anything (it's already been called). */
	return io_wait_(NULL, (void *)1, NULL);
}

int io_debug_io(int ret)
{
	/* Cache it for debugging; current changes. */
	struct io_conn *conn = current;
	int saved_errno = errno;

	if (!doing_debug_on(conn))
		return ret;

	/* These will all go linearly through the io_debug() path above. */
	switch (ret) {
	case -1:
		/* This will call io_debug above. */
		errno = saved_errno;
		io_close();
		break;
	case 0: /* Keep going with plan. */
		io_debug(conn->plan);
		break;
	case 1: /* Done: get next plan. */
		if (timeout_active(conn))
			backend_del_timeout(conn);
		/* In case they call io_duplex, clear our poll flags so
		 * both sides don't seem to be both doing read or write
		 * (See	assert(!mask || pfd->events != mask) in poll.c) */
		conn->plan.pollflag = 0;
		conn->plan.next(conn, conn->plan.next_arg);
		break;
	default:
		abort();
	}

	/* Normally-invalid value, used for sanity check. */
	return 2;
}

/* Counterpart to io_plan_no_debug(), called in macros in io.h */
static void io_plan_debug_again(void)
{
	io_plan_nodebug = false;
}
#else
static void io_plan_debug_again(void)
{
}
#endif

struct io_listener *io_new_listener_(int fd,
				     void (*init)(int fd, void *arg),
				     void *arg)
{
	struct io_listener *l = io_alloc.alloc(sizeof(*l));

	if (!l)
		return NULL;

	l->fd.listener = true;
	l->fd.fd = fd;
	l->init = init;
	l->arg = arg;
	if (!add_listener(l)) {
		io_alloc.free(l);
		return NULL;
	}
	return l;
}

void io_close_listener(struct io_listener *l)
{
	close(l->fd.fd);
	del_listener(l);
	io_alloc.free(l);
}

struct io_conn *io_new_conn_(int fd, struct io_plan plan)
{
	struct io_conn *conn = io_alloc.alloc(sizeof(*conn));

	io_plan_debug_again();

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->plan = plan;
	conn->finish = NULL;
	conn->finish_arg = NULL;
	conn->duplex = NULL;
	conn->timeout = NULL;
	if (!add_conn(conn)) {
		io_alloc.free(conn);
		return NULL;
	}
	return conn;
}

void io_set_finish_(struct io_conn *conn,
		    void (*finish)(struct io_conn *, void *),
		    void *arg)
{
	conn->finish = finish;
	conn->finish_arg = arg;
}

struct io_conn *io_duplex_(struct io_conn *old, struct io_plan plan)
{
	struct io_conn *conn;

	io_plan_debug_again();

	assert(!old->duplex);

	conn = io_alloc.alloc(sizeof(*conn));
	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = old->fd.fd;
	conn->plan = plan;
	conn->duplex = old;
	conn->finish = NULL;
	conn->finish_arg = NULL;
	conn->timeout = NULL;
	if (!add_duplex(conn)) {
		io_alloc.free(conn);
		return NULL;
	}
	old->duplex = conn;
	return conn;
}

bool io_timeout_(struct io_conn *conn, struct timerel t,
		 struct io_plan (*cb)(struct io_conn *, void *), void *arg)
{
	assert(cb);

	if (!conn->timeout) {
		conn->timeout = io_alloc.alloc(sizeof(*conn->timeout));
		if (!conn->timeout)
			return false;
	} else
		assert(!timeout_active(conn));

	conn->timeout->next = cb;
	conn->timeout->next_arg = arg;
	backend_add_timeout(conn, t);
	return true;
}

/* Always done: call the next thing. */
static int do_always(int fd, struct io_plan *plan)
{
	return 1;
}

struct io_plan io_always_(struct io_plan (*cb)(struct io_conn *, void *),
			  void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.io = do_always;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLALWAYS;

	return plan;
}

/* Returns true if we're finished. */
static int do_write(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u1.cp, plan->u2.s);
	if (ret < 0)
		return io_debug_io(-1);

	plan->u1.cp += ret;
	plan->u2.s -= ret;
	return io_debug_io(plan->u2.s == 0);
}

/* Queue some data to be written. */
struct io_plan io_write_(const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan plan;

	assert(cb);

	if (len == 0)
		return io_always_(cb, arg);

	plan.u1.const_vp = data;
	plan.u2.s = len;
	plan.io = do_write;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;

	return plan;
}

static int do_read(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u1.cp, plan->u2.s);
	if (ret <= 0)
		return io_debug_io(-1);

	plan->u1.cp += ret;
	plan->u2.s -= ret;
	return io_debug_io(plan->u2.s == 0);
}

/* Queue a request to read into a buffer. */
struct io_plan io_read_(void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg)
{
	struct io_plan plan;

	assert(cb);

	if (len == 0)
		return io_always_(cb, arg);

	plan.u1.cp = data;
	plan.u2.s = len;
	plan.io = do_read;
	plan.next = cb;
	plan.next_arg = arg;

	plan.pollflag = POLLIN;

	return plan;
}

static int do_read_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u1.cp, *(size_t *)plan->u2.vp);
	if (ret <= 0)
		return io_debug_io(-1);

	*(size_t *)plan->u2.vp = ret;
	return io_debug_io(1);
}

/* Queue a partial request to read into a buffer. */
struct io_plan io_read_partial_(void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg)
{
	struct io_plan plan;

	assert(cb);

	if (*len == 0)
		return io_always_(cb, arg);

	plan.u1.cp = data;
	plan.u2.vp = len;
	plan.io = do_read_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;

	return plan;
}

static int do_write_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u1.cp, *(size_t *)plan->u2.vp);
	if (ret < 0)
		return io_debug_io(-1);

	*(size_t *)plan->u2.vp = ret;
	return io_debug_io(1);
}

/* Queue a partial write request. */
struct io_plan io_write_partial_(const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn*, void *),
				 void *arg)
{
	struct io_plan plan;

	assert(cb);

	if (*len == 0)
		return io_always_(cb, arg);

	plan.u1.const_vp = data;
	plan.u2.vp = len;
	plan.io = do_write_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;

	return plan;
}

static int already_connected(int fd, struct io_plan *plan)
{
	return io_debug_io(1);
}

static int do_connect(int fd, struct io_plan *plan)
{
	int err, ret;
	socklen_t len = sizeof(err);

	/* Has async connect finished? */
	ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len);
	if (ret < 0)
		return -1;

	if (err == 0) {
		/* Restore blocking if it was initially. */
		fcntl(fd, F_SETFD, plan->u1.s);
		return 1;
	}
	return 0;
}

struct io_plan io_connect_(int fd, const struct addrinfo *addr,
			   struct io_plan (*cb)(struct io_conn*, void *),
			   void *arg)
{
	struct io_plan plan;

	assert(cb);

	plan.next = cb;
	plan.next_arg = arg;

	/* Save old flags, set nonblock if not already. */
	plan.u1.s = fcntl(fd, F_GETFD);
	fcntl(fd, F_SETFD, plan.u1.s | O_NONBLOCK);

	/* Immediate connect can happen. */
	if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
		/* Dummy will be called immediately. */
		plan.pollflag = POLLOUT;
		plan.io = already_connected;
	} else {
		if (errno != EINPROGRESS)
			return io_close_();

		plan.pollflag = POLLIN;
		plan.io = do_connect;
	}
	return plan;
}

struct io_plan io_wait_(const void *wait,
			struct io_plan (*cb)(struct io_conn *, void*),
			void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.pollflag = 0;
	plan.io = NULL;
	plan.next = cb;
	plan.next_arg = arg;

	plan.u1.const_vp = wait;

	return plan;
}

void io_wake(const void *wait)
{
	backend_wait_changed(wait);
}

void io_ready(struct io_conn *conn)
{
	/* Beware io_close_other! */
	if (!conn->plan.next)
		return;

	set_current(conn);
	switch (conn->plan.io(conn->fd.fd, &conn->plan)) {
	case -1: /* Failure means a new plan: close up. */
		conn->plan = io_close();
		backend_plan_changed(conn);
		break;
	case 0: /* Keep going with plan. */
		break;
	case 1: /* Done: get next plan. */
		if (timeout_active(conn))
			backend_del_timeout(conn);
		/* In case they call io_duplex, clear our poll flags so
		 * both sides don't seem to be both doing read or write
		 * (See	assert(!mask || pfd->events != mask) in poll.c) */
		conn->plan.pollflag = 0;
		conn->plan = conn->plan.next(conn, conn->plan.next_arg);
		backend_plan_changed(conn);
	}
	set_current(NULL);
}

/* Close the connection, we're done. */
struct io_plan io_close_(void)
{
	struct io_plan plan;

	plan.pollflag = 0;
	/* This means we're closing. */
	plan.next = NULL;
	plan.u1.s = errno;

	return plan;
}

struct io_plan io_close_cb(struct io_conn *conn, void *arg)
{
	return io_close();
}

void io_close_other(struct io_conn *conn)
{
	/* Don't close if already closing! */
	if (conn->plan.next) {
		conn->plan = io_close_();
		backend_plan_changed(conn);
	}
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_plan io_break_(void *ret, struct io_plan plan)
{
	io_plan_debug_again();

	assert(ret);
	io_loop_return = ret;

	return plan;
}

static struct io_plan io_never_called(struct io_conn *conn, void *arg)
{
	abort();
}

struct io_plan io_never(void)
{
	return io_always_(io_never_called, NULL);
}

int io_conn_fd(const struct io_conn *conn)
{
	return conn->fd.fd;
}

void io_set_alloc(void *(*allocfn)(size_t size),
		  void *(*reallocfn)(void *ptr, size_t size),
		  void (*freefn)(void *ptr))
{
	io_alloc.alloc = allocfn;
	io_alloc.realloc = reallocfn;
	io_alloc.free = freefn;
}
