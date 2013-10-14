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
			     struct io_plan *(*start)(struct io_conn *, void *),
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
	conn->fd.next = start;
	conn->fd.finish = finish;
	conn->fd.finish_arg = conn->fd.next_arg = arg;
	conn->state = NEXT;
	conn->duplex = old;
	conn->timeout = NULL;
	if (!add_duplex(conn)) {
		free(conn);
		return NULL;
	}
	old->duplex = conn;
	return conn;
}

/* Convenient token which only we can produce. */
static inline struct io_next *to_ionext(struct io_conn *conn)
{
	return (struct io_next *)conn;
}

static inline struct io_plan *to_ioplan(enum io_state state)
{
	return (struct io_plan *)(long)state;
}

static inline struct io_conn *from_ionext(struct io_next *next)
{
	return (struct io_conn *)next;
}

struct io_next *io_next_(struct io_conn *conn,
			 struct io_plan *(*next)(struct io_conn *, void *),
			 void *arg)
{
	conn->fd.next = next;
	conn->fd.next_arg = arg;

	return to_ionext(conn);
}

bool io_timeout_(struct io_conn *conn, struct timespec ts,
		 struct io_plan *(*next)(struct io_conn *, void *), void *arg)
{
	if (!conn->timeout) {
		conn->timeout = malloc(sizeof(*conn->timeout));
		if (!conn->timeout)
			return false;
	} else
		assert(!timeout_active(conn));

	conn->timeout->next = next;
	conn->timeout->next_arg = arg;
	backend_add_timeout(conn, ts);
	return true;
}

/* Queue some data to be written. */
struct io_plan *io_write(const void *data, size_t len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.write.buf = data;
	conn->u.write.len = len;
	return to_ioplan(WRITE);
}

/* Queue a request to read into a buffer. */
struct io_plan *io_read(void *data, size_t len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.read.buf = data;
	conn->u.read.len = len;
	return to_ioplan(READ);
}

/* Queue a partial request to read into a buffer. */
struct io_plan *io_read_partial(void *data, size_t *len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.readpart.buf = data;
	conn->u.readpart.lenp = len;
	return to_ioplan(READPART);
}

/* Queue a partial write request. */
struct io_plan *io_write_partial(const void *data, size_t *len, struct io_next *next)
{
	struct io_conn *conn = from_ionext(next);
	conn->u.writepart.buf = data;
	conn->u.writepart.lenp = len;
	return to_ioplan(WRITEPART);
}

struct io_plan *io_idle(struct io_conn *conn)
{
	return to_ioplan(IDLE);
}

void io_wake_(struct io_conn *conn,
	      struct io_plan *(*next)(struct io_conn *, void *), void *arg)

{
	/* It might have finished, but we haven't called its finish() yet. */
	if (conn->state == FINISHED)
		return;
	assert(conn->state == IDLE);
	conn->fd.next = next;
	conn->fd.next_arg = arg;
	backend_set_state(conn, to_ioplan(NEXT));
}

static struct io_plan *do_next(struct io_conn *conn)
{
	if (timeout_active(conn))
		backend_del_timeout(conn);
	return conn->fd.next(conn, conn->fd.next_arg);
}

struct io_plan *do_ready(struct io_conn *conn)
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
	return to_ioplan(conn->state);
}

/* Useful next functions. */
/* Close the connection, we're done. */
struct io_plan *io_close(struct io_conn *conn, void *arg)
{
	return to_ioplan(FINISHED);
}

/* Exit the loop, returning this (non-NULL) arg. */
struct io_plan *io_break(void *arg, struct io_next *next)
{
	io_loop_return = arg;

	return to_ioplan(NEXT);
}
