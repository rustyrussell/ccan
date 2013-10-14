/* Licensed under BSD-MIT - see LICENSE file for details */
#include "io.h"
#include "backend.h"
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

static size_t num_fds = 0, max_fds = 0, num_next = 0, num_finished = 0, num_waiting = 0;
static struct pollfd *pollfds = NULL;
static struct fd **fds = NULL;

static bool add_fd(struct fd *fd, short events)
{
	if (num_fds + 1 > max_fds) {
		struct pollfd *newpollfds;
		struct fd **newfds;
		size_t num = max_fds ? max_fds * 2 : 8;

		newpollfds = realloc(pollfds, sizeof(*newpollfds) * num);
		if (!newpollfds)
			return false;
		pollfds = newpollfds;
		newfds = realloc(fds, sizeof(*newfds) * num);
		if (!newfds)
			return false;
		fds = newfds;
		max_fds = num;
	}

	pollfds[num_fds].fd = fd->fd;
	pollfds[num_fds].events = events;
	pollfds[num_fds].revents = 0; /* In case we're iterating now */
	fds[num_fds] = fd;
	fd->backend_info = num_fds;
	num_fds++;
	return true;
}

static void del_fd(struct fd *fd)
{
	size_t n = fd->backend_info;

	assert(n != -1);
	assert(n < num_fds);
	if (n != num_fds - 1) {
		/* Move last one over us. */
		pollfds[n] = pollfds[num_fds-1];
		fds[n] = fds[num_fds-1];
		assert(fds[n]->backend_info == num_fds-1);
		fds[n]->backend_info = n;
	} else if (num_fds == 1) {
		/* Free everything when no more fds. */
		free(pollfds);
		free(fds);
		pollfds = NULL;
		fds = NULL;
		max_fds = 0;
	}
	num_fds--;
	fd->backend_info = -1;
	close(fd->fd);
}

bool add_listener(struct io_listener *l)
{
	if (!add_fd(&l->fd, POLLIN))
		return false;
	num_waiting++;
	return true;
}

bool add_conn(struct io_conn *c)
{
	if (!add_fd(&c->fd, 0))
		return false;
	num_next++;
	return true;
}

bool add_duplex(struct io_conn *c)
{
	c->fd.backend_info = c->duplex->fd.backend_info;
	num_next++;
	return true;
}

static void del_conn(struct io_conn *conn)
{
	if (conn->fd.finish)
		conn->fd.finish(conn, conn->fd.finish_arg);
	if (conn->duplex) {
		/* In case fds[] pointed to the other one. */
		fds[conn->fd.backend_info] = &conn->duplex->fd;
		conn->duplex->duplex = NULL;
	} else
		del_fd(&conn->fd);
	if (conn->state == FINISHED)
		num_finished--;
	else if (conn->state == NEXT)
		num_next--;
}

void del_listener(struct io_listener *l)
{
	del_fd(&l->fd);
}

static int pollmask(enum io_state state)
{
	switch (state) {
	case READ:
	case READPART:
		return POLLIN;
	case WRITE:
	case WRITEPART:
		return POLLOUT;
	default:
		return 0;
	}
}

void backend_set_state(struct io_conn *conn, struct io_op *op)
{
	enum io_state state = from_ioop(op);
	struct pollfd *pfd = &pollfds[conn->fd.backend_info];

	if (pfd->events)
		num_waiting--;

	pfd->events = pollmask(state);
	if (conn->duplex) {
		int mask = pollmask(conn->duplex->state);
		/* You can't *both* read/write. */
		assert(!mask || pfd->events != mask);
		pfd->events |= mask;
	}
	if (pfd->events)
		num_waiting++;

	if (state == NEXT)
		num_next++;
	else if (state == FINISHED)
		num_finished++;

	conn->state = state;
}

static void accept_conn(struct io_listener *l)
{
	struct io_conn *c;
	int fd = accept(l->fd.fd, NULL, NULL);

	/* FIXME: What to do here? */
	if (fd < 0)
		return;
	c = io_new_conn(fd, l->fd.next, l->fd.finish, l->fd.next_arg);
	if (!c) {
		close(fd);
		return;
	}
}

/* It's OK to miss some, as long as we make progress. */
static void finish_and_next(bool finished_only)
{
	unsigned int i;

	for (i = 0; !io_loop_return && i < num_fds; i++) {
		struct io_conn *c, *duplex;

		if (!num_finished) {
			if (finished_only || num_next == 0)
				break;
		}
		if (fds[i]->listener)
			continue;
		c = (void *)fds[i];
		for (duplex = c->duplex; c; c = duplex, duplex = NULL) {
			if (c->state == FINISHED) {
				del_conn(c);
				free(c);
				i--;
			} else if (!finished_only && c->state == NEXT) {
				backend_set_state(c,
						  c->fd.next(c,
							     c->fd.next_arg));
				num_next--;
			}
		}
	}
}

static void ready(struct io_conn *c)
{
	backend_set_state(c, do_ready(c));
}

/* This is the main loop. */
void *io_loop(void)
{
	void *ret;

	while (!io_loop_return) {
		int i, r;

		if (num_finished || num_next) {
			finish_and_next(false);
			/* Could have started/finished more. */
			continue;
		}

		if (num_fds == 0)
			break;

		/* You can't tell them all to go to sleep! */
		assert(num_waiting);

		r = poll(pollfds, num_fds, -1);
		if (r < 0)
			break;

		for (i = 0; i < num_fds && !io_loop_return; i++) {
			struct io_conn *c = (void *)fds[i];
			int events = pollfds[i].revents;

			if (fds[i]->listener) {
				if (events & POLLIN)
					accept_conn((void *)c);
			} else if (events & (POLLIN|POLLOUT)) {
				if (c->duplex) {
					int mask = pollmask(c->duplex->state);
					if (events & mask) {
						ready(c->duplex);
						events &= ~mask;
						if (!(events&(POLLIN|POLLOUT)))
							continue;
					}
				}
				ready(c);
			} else if (events & POLLHUP) {
				backend_set_state(c, io_close(c, NULL));
				if (c->duplex)
					backend_set_state(c->duplex,
							  io_close(c->duplex,
								   NULL));
			}

		}
	}

	while (num_finished)
		finish_and_next(true);

	ret = io_loop_return;
	io_loop_return = NULL;
	return ret;
}
