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

struct io_listener *io_new_listener_(const tal_t *ctx, int fd,
				     struct io_plan *(*init)(struct io_conn *,
							     void *),
				     void *arg)
{
	struct io_listener *l = tal(ctx, struct io_listener);
	if (!l)
		return NULL;

	l->fd.listener = true;
	l->fd.fd = fd;
	l->init = init;
	l->arg = arg;
	l->ctx = ctx;
	if (!add_listener(l))
		return tal_free(l);
	return l;
}

void io_close_listener(struct io_listener *l)
{
	close(l->fd.fd);
	del_listener(l);
	tal_free(l);
}

static struct io_plan *io_never_called(struct io_conn *conn, void *arg)
{
	abort();
}

static void next_plan(struct io_conn *conn, struct io_plan *plan)
{
	struct io_plan *(*next)(struct io_conn *, void *arg);

	next = plan->next;

	plan->status = IO_UNSET;
	plan->io = NULL;
	plan->next = io_never_called;

	plan = next(conn, plan->next_arg);

	/* It should have set a plan inside this conn. */
	assert(plan == &conn->plan[IO_IN]
	       || plan == &conn->plan[IO_OUT]);
	assert(conn->plan[IO_IN].status != IO_UNSET
	       || conn->plan[IO_OUT].status != IO_UNSET);

	backend_new_plan(conn);
}

struct io_conn *io_new_conn_(const tal_t *ctx, int fd,
			     struct io_plan *(*init)(struct io_conn *, void *),
			     void *arg)
{
	struct io_conn *conn = tal(ctx, struct io_conn);

	if (!conn)
		return NULL;

	conn->fd.listener = false;
	conn->fd.fd = fd;
	conn->finish = NULL;
	conn->finish_arg = NULL;
	conn->list = NULL;

	if (!add_conn(conn))
		return tal_free(conn);

	/* We start with out doing nothing, and in doing our init. */
	conn->plan[IO_OUT].status = IO_UNSET;

	conn->plan[IO_IN].next = init;
	conn->plan[IO_IN].next_arg = arg;
	next_plan(conn, &conn->plan[IO_IN]);

	return conn;
}

void io_set_finish_(struct io_conn *conn,
		    void (*finish)(struct io_conn *, void *),
		    void *arg)
{
	conn->finish = finish;
	conn->finish_arg = arg;
}

struct io_plan *io_get_plan(struct io_conn *conn, enum io_direction dir)
{
	assert(conn->plan[dir].status == IO_UNSET);

	conn->plan[dir].status = IO_POLLING;
	return &conn->plan[dir];
}

static struct io_plan *set_always(struct io_conn *conn,
				  struct io_plan *plan,
				  struct io_plan *(*next)(struct io_conn *,
							  void *),
				  void *arg)
{
	plan->next = next;
	plan->next_arg = arg;
	plan->status = IO_ALWAYS;

	backend_new_always(conn);
	return plan;
}

struct io_plan *io_always_(struct io_conn *conn,
			   struct io_plan *(*next)(struct io_conn *, void *),
			   void *arg)
{
	struct io_plan *plan;

	/* If we're duplex, we want this on the current plan.  Otherwise,
	 * doesn't matter. */
	if (conn->plan[IO_IN].status == IO_UNSET)
		plan = io_get_plan(conn, IO_IN);
	else
		plan = io_get_plan(conn, IO_OUT);

	assert(next);
	set_always(conn, plan, next, arg);

	return plan;
}

static int do_write(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u1.cp, plan->u2.s);
	if (ret < 0)
		return -1;

	plan->u1.cp += ret;
	plan->u2.s -= ret;
	return plan->u2.s == 0;
}

/* Queue some data to be written. */
struct io_plan *io_write_(struct io_conn *conn, const void *data, size_t len,
			  struct io_plan *(*next)(struct io_conn *, void *),
			  void *arg)
{
	struct io_plan *plan = io_get_plan(conn, IO_OUT);

	assert(next);

	if (len == 0)
		return set_always(conn, plan, next, arg);

	plan->u1.const_vp = data;
	plan->u2.s = len;
	plan->io = do_write;
	plan->next = next;
	plan->next_arg = arg;

	return plan;
}

static int do_read(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u1.cp, plan->u2.s);
	if (ret <= 0)
		return -1;

	plan->u1.cp += ret;
	plan->u2.s -= ret;
	return plan->u2.s == 0;
}

/* Queue a request to read into a buffer. */
struct io_plan *io_read_(struct io_conn *conn,
			 void *data, size_t len,
			 struct io_plan *(*next)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan *plan = io_get_plan(conn, IO_IN);

	assert(next);

	if (len == 0)
		return set_always(conn, plan, next, arg);

	plan->u1.cp = data;
	plan->u2.s = len;
	plan->io = do_read;
	plan->next = next;
	plan->next_arg = arg;

	return plan;
}

static int do_read_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = read(fd, plan->u1.cp, *(size_t *)plan->u2.vp);
	if (ret <= 0)
		return -1;

	*(size_t *)plan->u2.vp = ret;
	return 1;
}

/* Queue a partial request to read into a buffer. */
struct io_plan *io_read_partial_(struct io_conn *conn,
				 void *data, size_t maxlen, size_t *len,
				 struct io_plan *(*next)(struct io_conn *,
							 void *),
				 void *arg)
{
	struct io_plan *plan = io_get_plan(conn, IO_IN);

	assert(next);

	if (maxlen == 0)
		return set_always(conn, plan, next, arg);

	plan->u1.cp = data;
	/* We store the max len in here temporarily. */
	*len = maxlen;
	plan->u2.vp = len;
	plan->io = do_read_partial;
	plan->next = next;
	plan->next_arg = arg;

	return plan;
}

static int do_write_partial(int fd, struct io_plan *plan)
{
	ssize_t ret = write(fd, plan->u1.cp, *(size_t *)plan->u2.vp);
	if (ret < 0)
		return -1;

	*(size_t *)plan->u2.vp = ret;
	return 1;
}

/* Queue a partial write request. */
struct io_plan *io_write_partial_(struct io_conn *conn,
				  const void *data, size_t maxlen, size_t *len,
				  struct io_plan *(*next)(struct io_conn *,
							  void*),
				  void *arg)
{
	struct io_plan *plan = io_get_plan(conn, IO_OUT);

	assert(next);

	if (maxlen == 0)
		return set_always(conn, plan, next, arg);

	plan->u1.const_vp = data;
	/* We store the max len in here temporarily. */
	*len = maxlen;
	plan->u2.vp = len;
	plan->io = do_write_partial;
	plan->next = next;
	plan->next_arg = arg;

	return plan;
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
		fcntl(fd, F_SETFL, plan->u1.s);
		return 1;
	} else if (err == EINPROGRESS)
		return 0;

	errno = err;
	return -1;
}

struct io_plan *io_connect_(struct io_conn *conn, const struct addrinfo *addr,
			    struct io_plan *(*next)(struct io_conn *, void *),
			    void *arg)
{
	struct io_plan *plan = io_get_plan(conn, IO_IN);
	int fd = io_conn_fd(conn);

	assert(next);

	/* Save old flags, set nonblock if not already. */
	plan->u1.s = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, plan->u1.s | O_NONBLOCK);

	/* Immediate connect can happen. */
	if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0)
		return set_always(conn, plan, next, arg);

	if (errno != EINPROGRESS)
		return io_close(conn);

	plan->next = next;
	plan->next_arg = arg;
	plan->io = do_connect;

	return plan;
}

struct io_plan *io_wait_(struct io_conn *conn,
			 const void *wait,
			 struct io_plan *(*next)(struct io_conn *, void *),
			 void *arg)
{
	struct io_plan *plan;

	/* If we're duplex, we want this on the current plan.  Otherwise,
	 * doesn't matter. */
	if (conn->plan[IO_IN].status == IO_UNSET)
		plan = io_get_plan(conn, IO_IN);
	else
		plan = io_get_plan(conn, IO_OUT);

	assert(next);

	plan->next = next;
	plan->next_arg = arg;
	plan->u1.const_vp = wait;
	plan->status = IO_WAITING;

	return plan;
}

void io_wake(const void *wait)
{
	backend_wake(wait);
}

static void do_plan(struct io_conn *conn, struct io_plan *plan)
{
	/* Someone else might have called io_close() on us. */
	if (plan->status == IO_CLOSING)
		return;

	/* We shouldn't have polled for this event if this wasn't true! */
	assert(plan->status == IO_POLLING);

	switch (plan->io(conn->fd.fd, plan)) {
	case -1:
		io_close(conn);
		break;
	case 0:
		break;
	case 1:
		next_plan(conn, plan);
		break;
	default:
		/* IO should only return -1, 0 or 1 */
		abort();
	}
}

void io_ready(struct io_conn *conn, int pollflags)
{
	if (pollflags & POLLIN)
		do_plan(conn, &conn->plan[IO_IN]);

	if (pollflags & POLLOUT)
		do_plan(conn, &conn->plan[IO_OUT]);
}

void io_do_always(struct io_conn *conn)
{
	if (conn->plan[IO_IN].status == IO_ALWAYS)
		next_plan(conn, &conn->plan[IO_IN]);

	if (conn->plan[IO_OUT].status == IO_ALWAYS)
		next_plan(conn, &conn->plan[IO_OUT]);
}

void io_do_wakeup(struct io_conn *conn, struct io_plan *plan)
{
	assert(plan->status == IO_WAITING);
	next_plan(conn, plan);
}

/* Close the connection, we're done. */
struct io_plan *io_close(struct io_conn *conn)
{
	/* Already closing?  Don't close twice. */
	if (conn->plan[IO_IN].status == IO_CLOSING)
		return &conn->plan[IO_IN];

	conn->plan[IO_IN].status = conn->plan[IO_OUT].status = IO_CLOSING;
	conn->plan[IO_IN].u1.s = errno;
	backend_new_closing(conn);

	return &conn->plan[IO_IN];
}

struct io_plan *io_close_cb(struct io_conn *conn, void *arg)
{
	return io_close(conn);
}

/* Exit the loop, returning this (non-NULL) arg. */
void io_break(const void *ret)
{
	assert(ret);
	io_loop_return = (void *)ret;
}

struct io_plan *io_never(struct io_conn *conn)
{
	return io_always(conn, io_never_called, NULL);
}

int io_conn_fd(const struct io_conn *conn)
{
	return conn->fd.fd;
}
