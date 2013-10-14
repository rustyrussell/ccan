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
				     struct io_plan *(*start)(struct io_conn *,
							      void *arg),
				     void (*finish)(struct io_conn *, void *),
				     void *arg)
{
	struct io_listener *l = malloc(sizeof(*l));

	if (!l)
		return NULL;

	l->fd.listener = true;
	l->fd.fd = fd;
	l->next = start;
	l->finish = finish;
	l->conn_arg = arg;
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
			     struct io_plan *(*start)(struct io_conn *, void *),
			     void (*finish)(struct io_conn *, void *),
			     void *arg)
{
	struct io_conn *conn = malloc(sizeof(*conn));

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->next = start;
	conn->finish = finish;
	conn->finish_arg = conn->next_arg = arg;
	conn->pollflag = 0;
	conn->state = IO_NEXT;
	conn->duplex = NULL;
	conn->timeout = NULL;
	if (!add_conn(conn)) {
		free(conn);
		return NULL;
	}
	return conn;
}

struct io_conn *io_duplex_(struct io_conn *old,
			     struct io_plan *(*start)(struct io_conn *, void *),
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
	conn->next = start;
	conn->finish = finish;
	conn->finish_arg = conn->next_arg = arg;
	conn->pollflag = 0;
	conn->state = IO_NEXT;
	conn->duplex = old;
	conn->timeout = NULL;
	if (!add_duplex(conn)) {
		free(conn);
		return NULL;
	}
	old->duplex = conn;
	return conn;
}

static inline struct io_plan *to_ioplan(enum io_state state)
{
	return (struct io_plan *)(long)state;
}

bool io_timeout_(struct io_conn *conn, struct timespec ts,
		 struct io_plan *(*cb)(struct io_conn *, void *), void *arg)
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
	ssize_t ret = write(conn->fd.fd, conn->u.write.buf, conn->u.write.len);
	if (ret < 0)
		return RESULT_CLOSE;

	conn->u.write.buf += ret;
	conn->u.write.len -= ret;
	if (conn->u.write.len == 0)
		return RESULT_FINISHED;
	else
		return RESULT_AGAIN;
}

/* Queue some data to be written. */
struct io_plan *io_write_(struct io_conn *conn, const void *data, size_t len,
			  struct io_plan *(*cb)(struct io_conn *, void *),
			  void *arg)
{
	conn->u.write.buf = data;
	conn->u.write.len = len;
	conn->io = do_write;
	conn->next = cb;
	conn->next_arg = arg;
	conn->pollflag = POLLOUT;
	return to_ioplan(IO_IO);
}

static enum io_result do_read(struct io_conn *conn)
{
	ssize_t ret = read(conn->fd.fd, conn->u.read.buf, conn->u.read.len);
	if (ret <= 0)
		return RESULT_CLOSE;
	conn->u.read.buf += ret;
	conn->u.read.len -= ret;
	if (conn->u.read.len == 0)
		return RESULT_FINISHED;
	else
		return RESULT_AGAIN;
}

/* Queue a request to read into a buffer. */
struct io_plan *io_read_(struct io_conn *conn, void *data, size_t len,
			 struct io_plan *(*cb)(struct io_conn *, void *),
			 void *arg)
{
	conn->u.read.buf = data;
	conn->u.read.len = len;
	conn->io = do_read;
	conn->next = cb;
	conn->next_arg = arg;
	conn->pollflag = POLLIN;
	return to_ioplan(IO_IO);
}

static enum io_result do_read_partial(struct io_conn *conn)
{
	ssize_t ret = read(conn->fd.fd, conn->u.readpart.buf,
			   *conn->u.readpart.lenp);
	if (ret <= 0)
		return RESULT_CLOSE;
	*conn->u.readpart.lenp = ret;
	return RESULT_FINISHED;
}

/* Queue a partial request to read into a buffer. */
struct io_plan *io_read_partial_(struct io_conn *conn, void *data, size_t *len,
				 struct io_plan *(*cb)(struct io_conn *, void *),
				 void *arg)
{
	conn->u.readpart.buf = data;
	conn->u.readpart.lenp = len;
	conn->io = do_read_partial;
	conn->next = cb;
	conn->next_arg = arg;
	conn->pollflag = POLLIN;
	return to_ioplan(IO_IO);
}

static enum io_result do_write_partial(struct io_conn *conn)
{
	ssize_t ret = write(conn->fd.fd, conn->u.writepart.buf,
			    *conn->u.writepart.lenp);
	if (ret < 0)
		return RESULT_CLOSE;
	*conn->u.writepart.lenp = ret;
	return RESULT_FINISHED;
}

/* Queue a partial write request. */
struct io_plan *io_write_partial_(struct io_conn *conn,
				  const void *data, size_t *len,
				  struct io_plan *(*cb)(struct io_conn*, void *),
				  void *arg)
{
	conn->u.writepart.buf = data;
	conn->u.writepart.lenp = len;
	conn->io = do_write_partial;
	conn->next = cb;
	conn->next_arg = arg;
	conn->pollflag = POLLOUT;
	return to_ioplan(IO_IO);
}

struct io_plan *io_idle(struct io_conn *conn)
{
	conn->pollflag = 0;
	return to_ioplan(IO_IDLE);
}

void io_wake_(struct io_conn *conn,
	      struct io_plan *(*fn)(struct io_conn *, void *), void *arg)

{
	/* It might have finished, but we haven't called its finish() yet. */
	if (conn->state == IO_FINISHED)
		return;
	assert(conn->state == IO_IDLE);
	conn->next = fn;
	conn->next_arg = arg;
	conn->state = IO_NEXT;
	backend_wakeup(conn);
}

static struct io_plan *do_next(struct io_conn *conn)
{
	if (timeout_active(conn))
		backend_del_timeout(conn);
	return conn->next(conn, conn->next_arg);
}

struct io_plan *do_ready(struct io_conn *conn)
{
	assert(conn->state == IO_IO);
	switch (conn->io(conn)) {
	case RESULT_CLOSE:
		return io_close(conn, NULL);
	case RESULT_FINISHED:
		return do_next(conn);
	case RESULT_AGAIN:
		return to_ioplan(conn->state);
	default:
		abort();
	}
}

/* Useful next functions. */
/* Close the connection, we're done. */
struct io_plan *io_close(struct io_conn *conn, void *arg)
{
	return to_ioplan(IO_FINISHED);
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_plan *io_break_(struct io_conn *conn, void *ret,
			  struct io_plan *(*fn)(struct io_conn *, void *),
			  void *arg)
{
	io_loop_return = ret;
	conn->next = fn;
	conn->next_arg = arg;

	return to_ioplan(IO_NEXT);
}
