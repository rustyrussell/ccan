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
#include <poll.h>

void *io_loop_return;

struct io_listener *io_new_listener_(int fd,
				     void (*init)(int fd, void *arg),
				     void *arg)
{
	struct io_listener *l = malloc(sizeof(*l));

	if (!l)
		return NULL;

	l->fd.listener = true;
	l->fd.fd = fd;
	l->init = init;
	l->arg = arg;
	if (!add_listener(l)) {
		free(l);
		return NULL;
	}
	return l;
}

void io_close_listener(struct io_listener *l)
{
	close(l->fd.fd);
	del_listener(l);
	free(l);
}

struct io_conn *io_new_conn_(int fd,
			     struct io_plan plan,
			     void (*finish)(struct io_conn *, void *),
			     void *arg)
{
	struct io_conn *conn = malloc(sizeof(*conn));

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->plan = plan;
	conn->finish = finish;
	conn->finish_arg = arg;
	conn->duplex = NULL;
	conn->timeout = NULL;
	if (!add_conn(conn)) {
		free(conn);
		return NULL;
	}
	return conn;
}

struct io_conn *io_duplex_(struct io_conn *old,
			   struct io_plan plan,
			   void (*finish)(struct io_conn *, void *),
			   void *arg)
{
	struct io_conn *conn;

	assert(!old->duplex);

	conn = malloc(sizeof(*conn));
	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = old->fd.fd;
	conn->plan = plan;
	conn->duplex = old;
	conn->finish = finish;
	conn->finish_arg = arg;
	conn->timeout = NULL;
	if (!add_duplex(conn)) {
		free(conn);
		return NULL;
	}
	old->duplex = conn;
	return conn;
}

bool io_timeout_(struct io_conn *conn, struct timespec ts,
		 struct io_plan (*cb)(struct io_conn *, void *), void *arg)
{
	assert(cb);

	if (!conn->timeout) {
		conn->timeout = malloc(sizeof(*conn->timeout));
		if (!conn->timeout)
			return false;
	} else
		assert(!timeout_active(conn));

	conn->timeout->next = cb;
	conn->timeout->next_arg = arg;
	backend_add_timeout(conn, ts);
	return true;
}

/* Returns true if we're finished. */
static bool do_write(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u.write.buf, plan->u.write.len);
	if (ret < 0) {
		/* Override next function to close us. */
		plan->next = io_close;
		return true;
	}

	plan->u.write.buf += ret;
	plan->u.write.len -= ret;
	return (plan->u.write.len == 0);
}

/* Queue some data to be written. */
struct io_plan io_write_(const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.u.write.buf = data;
	plan.u.write.len = len;
	plan.io = do_write;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;
	return plan;
}

static bool do_read(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u.read.buf, plan->u.read.len);
	if (ret <= 0) {
		/* Override next function to close us. */
		plan->next = io_close;
		return true;
	}

	plan->u.read.buf += ret;
	plan->u.read.len -= ret;
	return (plan->u.read.len == 0);
}

/* Queue a request to read into a buffer. */
struct io_plan io_read_(void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.u.read.buf = data;
	plan.u.read.len = len;
	plan.io = do_read;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;
	return plan;
}

static bool do_read_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u.readpart.buf, *plan->u.readpart.lenp);
	if (ret <= 0) {
		/* Override next function to close us. */
		plan->next = io_close;
		return true;
	}

	*plan->u.readpart.lenp = ret;
	return true;
}

/* Queue a partial request to read into a buffer. */
struct io_plan io_read_partial_(void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.u.readpart.buf = data;
	plan.u.readpart.lenp = len;
	plan.io = do_read_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;

	return plan;
}

static bool do_write_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u.writepart.buf, *plan->u.writepart.lenp);
	if (ret < 0) {
		/* Override next function to close us. */
		plan->next = io_close;
		return true;
	}

	*plan->u.writepart.lenp = ret;
	return true;
}

/* Queue a partial write request. */
struct io_plan io_write_partial_(const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn*, void *),
				 void *arg)
{
	struct io_plan plan;

	assert(cb);
	plan.u.writepart.buf = data;
	plan.u.writepart.lenp = len;
	plan.io = do_write_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;

	return plan;
}

struct io_plan io_idle(void)
{
	struct io_plan plan;

	plan.pollflag = 0;
	plan.io = NULL;
	/* Never called (overridded by io_wake), but NULL means closing */
	plan.next = io_close;

	return plan;
}

void io_wake(struct io_conn *conn, struct io_plan plan)

{
	/* It might be closing, but we haven't called its finish() yet. */
	if (!conn->plan.next)
		return;
	/* It was idle, right? */
	assert(!conn->plan.io);
	conn->plan = plan;
	backend_plan_changed(conn);
}

void io_ready(struct io_conn *conn)
{
	if (conn->plan.io(conn->fd.fd, &conn->plan)) {
		if (timeout_active(conn))
			backend_del_timeout(conn);
		conn->plan = conn->plan.next(conn, conn->plan.next_arg);
		backend_plan_changed(conn);
	}
}

/* Useful next functions. */
/* Close the connection, we're done. */
struct io_plan io_close(struct io_conn *conn, void *arg)
{
	struct io_plan plan;

	plan.pollflag = 0;
	/* This means we're closing. */
	plan.next = NULL;

	return plan;
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_plan io_break(void *ret, struct io_plan plan)
{
	assert(ret);
	io_loop_return = ret;

	return plan;
}
