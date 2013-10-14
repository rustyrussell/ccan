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
			     struct io_plan (*start)(struct io_conn *, void *),
			     void (*finish)(struct io_conn *, void *),
			     void *arg)
{
	struct io_conn *conn = malloc(sizeof(*conn));

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->plan.next = start;
	conn->finish = finish;
	conn->finish_arg = conn->plan.next_arg = arg;
	conn->plan.pollflag = 0;
	conn->plan.state = IO_NEXT;
	conn->duplex = NULL;
	conn->timeout = NULL;
	if (!add_conn(conn)) {
		free(conn);
		return NULL;
	}
	return conn;
}

struct io_conn *io_duplex_(struct io_conn *old,
			     struct io_plan (*start)(struct io_conn *, void *),
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
	conn->plan.next = start;
	conn->finish = finish;
	conn->finish_arg = conn->plan.next_arg = arg;
	conn->plan.pollflag = 0;
	conn->plan.state = IO_NEXT;
	conn->duplex = old;
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

static enum io_result do_write(struct io_conn *conn)
{
	ssize_t ret = write(conn->fd.fd, conn->plan.u.write.buf, conn->plan.u.write.len);
	if (ret < 0)
		return RESULT_CLOSE;

	conn->plan.u.write.buf += ret;
	conn->plan.u.write.len -= ret;
	if (conn->plan.u.write.len == 0)
		return RESULT_FINISHED;
	else
		return RESULT_AGAIN;
}

/* Queue some data to be written. */
struct io_plan io_write_(const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan plan;

	plan.u.write.buf = data;
	plan.u.write.len = len;
	plan.io = do_write;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;
	plan.state = IO_IO;
	return plan;
}

static enum io_result do_read(struct io_conn *conn)
{
	ssize_t ret = read(conn->fd.fd, conn->plan.u.read.buf,
			   conn->plan.u.read.len);
	if (ret <= 0)
		return RESULT_CLOSE;
	conn->plan.u.read.buf += ret;
	conn->plan.u.read.len -= ret;
	if (conn->plan.u.read.len == 0)
		return RESULT_FINISHED;
	else
		return RESULT_AGAIN;
}

/* Queue a request to read into a buffer. */
struct io_plan io_read_(void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg)
{
	struct io_plan plan;

	plan.u.read.buf = data;
	plan.u.read.len = len;
	plan.io = do_read;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;
	plan.state = IO_IO;
	return plan;
}

static enum io_result do_read_partial(struct io_conn *conn)
{
	ssize_t ret = read(conn->fd.fd, conn->plan.u.readpart.buf,
			   *conn->plan.u.readpart.lenp);
	if (ret <= 0)
		return RESULT_CLOSE;
	*conn->plan.u.readpart.lenp = ret;
	return RESULT_FINISHED;
}

/* Queue a partial request to read into a buffer. */
struct io_plan io_read_partial_(void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg)
{
	struct io_plan plan;

	plan.u.readpart.buf = data;
	plan.u.readpart.lenp = len;
	plan.io = do_read_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLIN;
	plan.state = IO_IO;

	return plan;
}

static enum io_result do_write_partial(struct io_conn *conn)
{
	ssize_t ret = write(conn->fd.fd, conn->plan.u.writepart.buf,
			    *conn->plan.u.writepart.lenp);
	if (ret < 0)
		return RESULT_CLOSE;
	*conn->plan.u.writepart.lenp = ret;
	return RESULT_FINISHED;
}

/* Queue a partial write request. */
struct io_plan io_write_partial_(const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn*, void *),
				 void *arg)
{
	struct io_plan plan;

	plan.u.writepart.buf = data;
	plan.u.writepart.lenp = len;
	plan.io = do_write_partial;
	plan.next = cb;
	plan.next_arg = arg;
	plan.pollflag = POLLOUT;
	plan.state = IO_IO;

	return plan;
}

struct io_plan io_idle(void)
{
	struct io_plan plan;

	plan.pollflag = 0;
	plan.state = IO_IDLE;

	return plan;
}

void io_wake_(struct io_conn *conn,
	      struct io_plan (*fn)(struct io_conn *, void *), void *arg)

{
	/* It might have finished, but we haven't called its finish() yet. */
	if (conn->plan.state == IO_FINISHED)
		return;
	assert(conn->plan.state == IO_IDLE);
	conn->plan.next = fn;
	conn->plan.next_arg = arg;
	conn->plan.pollflag = 0;
	conn->plan.state = IO_NEXT;
	backend_wakeup(conn);
}

static struct io_plan do_next(struct io_conn *conn)
{
	if (timeout_active(conn))
		backend_del_timeout(conn);
	return conn->plan.next(conn, conn->plan.next_arg);
}

struct io_plan do_ready(struct io_conn *conn)
{
	assert(conn->plan.state == IO_IO);
	switch (conn->plan.io(conn)) {
	case RESULT_CLOSE:
		return io_close(conn, NULL);
	case RESULT_FINISHED:
		return do_next(conn);
	case RESULT_AGAIN:
		return conn->plan;
	default:
		abort();
	}
}

/* Useful next functions. */
/* Close the connection, we're done. */
struct io_plan io_close(struct io_conn *conn, void *arg)
{
	struct io_plan plan;

	plan.state = IO_FINISHED;
	plan.pollflag = 0;

	return plan;
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_plan io_break_(void *ret,
			 struct io_plan (*fn)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan plan;

	io_loop_return = ret;

	plan.state = IO_NEXT;
	plan.pollflag = 0;
	plan.next = fn;
	plan.next_arg = arg;

	return plan;
}
