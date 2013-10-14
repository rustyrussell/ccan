/* Licensed under BSD-MIT - see LICENSE file for details */
#include "io.h"
#include "backend.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>

void *io_loop_return;

struct io_listener *io_new_listener_(int fd,
				     struct io_op *(*start)(struct io_conn *,
							    void *arg),
				     void (*finish)(struct io_conn *, void *),
				     void *arg)
{
	struct io_listener *l = malloc(sizeof(*l));

	if (!l)
		return NULL;

	l->fd.listener = true;
	l->fd.fd = fd;
	l->fd.next = start;
	l->fd.finish = finish;
	l->fd.finish_arg = l->fd.next_arg = arg;
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
			     struct io_op *(*start)(struct io_conn *, void *),
			     void (*finish)(struct io_conn *, void *),
			     void *arg)
{
	struct io_conn *conn = malloc(sizeof(*conn));

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->fd.next = start;
	conn->fd.finish = finish;
	conn->fd.finish_arg = conn->fd.next_arg = arg;
	conn->state = NEXT;
	if (!add_conn(conn)) {
		free(conn);
		return NULL;
	}
	return conn;
}

/* Convenient token which only we can produce. */
static inline struct io_next *to_ionext(struct io_conn *conn)
{
	return (struct io_next *)conn;
}

static inline struct io_op *to_ioop(enum io_state state)
{
	return (struct io_op *)(long)state;
}

static inline struct io_conn *from_ionext(struct io_next *next)
{
	return (struct io_conn *)next;
}

struct io_next *io_next_(struct io_conn *conn,
			 struct io_op *(*next)(struct io_conn *, void *),
			 void *arg)
{
	conn->fd.next = next;
	conn->fd.next_arg = arg;

	return to_ionext(conn);
}

/* Queue some data to be written. */
struct io_op *io_write(const void *data, size_t len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.write.buf = data;
	conn->u.write.len = len;
	return to_ioop(WRITE);
}

/* Queue a request to read into a buffer. */
struct io_op *io_read(void *data, size_t len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.read.buf = data;
	conn->u.read.len = len;
	return to_ioop(READ);
}

/* Queue a partial request to read into a buffer. */
struct io_op *io_read_partial(void *data, size_t *len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.readpart.buf = data;
	conn->u.readpart.lenp = len;
	return to_ioop(READPART);
}

/* Queue a partial write request. */
struct io_op *io_write_partial(const void *data, size_t *len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.writepart.buf = data;
	conn->u.writepart.lenp = len;
	return to_ioop(WRITEPART);
}

struct io_op *io_idle(struct io_conn *conn)
{
	return to_ioop(IDLE);
}

void io_wake_(struct io_conn *conn,
	      struct io_op *(*next)(struct io_conn *, void *), void *arg)

{
	/* It might have finished, but we haven't called its finish() yet. */
	if (conn->state == FINISHED)
		return;
	assert(conn->state == IDLE);
	conn->fd.next = next;
	conn->fd.next_arg = arg;
	backend_set_state(conn, to_ioop(NEXT));
}

static struct io_op *do_next(struct io_conn *conn)
{
	return conn->fd.next(conn, conn->fd.next_arg);
}

struct io_op *do_writeable(struct io_conn *conn)
{
	ssize_t ret;
	bool finished;

	switch (conn->state) {
	case WRITE:
		ret = write(conn->fd.fd, conn->u.write.buf, conn->u.write.len);
		if (ret < 0)
			return io_close(conn, NULL);
		conn->u.write.buf += ret;
		conn->u.write.len -= ret;
		finished = (conn->u.write.len == 0);
		break;
	case WRITEPART:
		ret = write(conn->fd.fd, conn->u.writepart.buf,
			    *conn->u.writepart.lenp);
		if (ret < 0)
			return io_close(conn, NULL);
		*conn->u.writepart.lenp = ret;
		finished = true;
		break;
	default:
		/* Shouldn't happen. */
		abort();
	}

	if (finished)
		return do_next(conn);
	return to_ioop(conn->state);
}

struct io_op *do_readable(struct io_conn *conn)
{
	ssize_t ret;
	bool finished;

	switch (conn->state) {
	case READ:
		ret = read(conn->fd.fd, conn->u.read.buf, conn->u.read.len);
		if (ret <= 0)
			return io_close(conn, NULL);
		conn->u.read.buf += ret;
		conn->u.read.len -= ret;
		finished = (conn->u.read.len == 0);
		break;
	case READPART:
		ret = read(conn->fd.fd, conn->u.readpart.buf,
			    *conn->u.readpart.lenp);
		if (ret <= 0)
			return io_close(conn, NULL);
		*conn->u.readpart.lenp = ret;
		finished = true;
		break;
	default:
		/* Shouldn't happen. */
		abort();
	}

	if (finished)
		return do_next(conn);
	return to_ioop(conn->state);
}

/* Useful next functions. */
/* Close the connection, we're done. */
struct io_op *io_close(struct io_conn *conn, void *arg)
{
	return to_ioop(FINISHED);
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_op *io_break(void *arg, struct io_next *next)
{
	io_loop_return = arg;

	return to_ioop(NEXT);
}
