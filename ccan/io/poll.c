/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include "io.h"
#include "backend.h"
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>

static size_t num_fds = 0, max_fds = 0, num_next = 0, num_finished = 0, num_waiting = 0;
static struct pollfd *pollfds = NULL;
static struct fd **fds = NULL;
static struct timers timeouts;

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
	if (conn->finish)
		conn->finish(conn, conn->finish_arg);
	if (timeout_active(conn))
		backend_del_timeout(conn);
	free(conn->timeout);
	if (conn->duplex) {
		/* In case fds[] pointed to the other one. */
		fds[conn->fd.backend_info] = &conn->duplex->fd;
		conn->duplex->duplex = NULL;
	} else
		del_fd(&conn->fd);
	if (conn->plan.state == IO_FINISHED)
		num_finished--;
	else if (conn->plan.state == IO_NEXT)
		num_next--;
}

void del_listener(struct io_listener *l)
{
	del_fd(&l->fd);
}

static void backend_set_state(struct io_conn *conn, struct io_plan plan)
{
	struct pollfd *pfd = &pollfds[conn->fd.backend_info];

	if (pfd->events)
		num_waiting--;

	pfd->events = plan.pollflag;
	if (conn->duplex) {
		int mask = conn->duplex->plan.pollflag;
		/* You can't *both* read/write. */
		assert(!mask || pfd->events != mask);
		pfd->events |= mask;
	}
	if (pfd->events)
		num_waiting++;

	if (plan.state == IO_NEXT)
		num_next++;
	else if (plan.state == IO_FINISHED)
		num_finished++;

	conn->plan = plan;
}

void backend_wakeup(struct io_conn *conn)
{
	num_next++;
}

static void accept_conn(struct io_listener *l)
{
	struct io_conn *c;
	int fd = accept(l->fd.fd, NULL, NULL);

	/* FIXME: What to do here? */
	if (fd < 0)
		return;
	c = io_new_conn(fd, l->next, l->finish, l->conn_arg);
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
			if (c->plan.state == IO_FINISHED) {
				del_conn(c);
				free(c);
				i--;
			} else if (!finished_only && c->plan.state == IO_NEXT) {
				backend_set_state(c, c->plan.next(c, c->plan.next_arg));
				num_next--;
			}
		}
	}
}

static void ready(struct io_conn *c)
{
	backend_set_state(c, do_ready(c));
}

void backend_add_timeout(struct io_conn *conn, struct timespec duration)
{
	if (!timeouts.base)
		timers_init(&timeouts, time_now());
	timer_add(&timeouts, &conn->timeout->timer,
		  time_add(time_now(), duration));
	conn->timeout->conn = conn;
}

void backend_del_timeout(struct io_conn *conn)
{
	assert(conn->timeout->conn == conn);
	timer_del(&timeouts, &conn->timeout->timer);
	conn->timeout->conn = NULL;
}

/* This is the main loop. */
void *io_loop(void)
{
	void *ret;

	while (!io_loop_return) {
		int i, r, timeout = INT_MAX;
		struct timespec now;

		if (timeouts.base) {
			struct timespec first;
			struct list_head expired;
			struct io_timeout *t;

			now = time_now();

			/* Call functions for expired timers. */
			timers_expire(&timeouts, now, &expired);
			while ((t = list_pop(&expired, struct io_timeout, timer.list))) {
				struct io_conn *conn = t->conn;
				/* Clear, in case timer re-adds */
				t->conn = NULL;
				backend_set_state(conn, t->next(conn, t->next_arg));
			}

			/* Now figure out how long to wait for the next one. */
			if (timer_earliest(&timeouts, &first)) {
				uint64_t f = time_to_msec(time_sub(first, now));
				if (f < INT_MAX)
					timeout = f;
			}
		}

		if (num_finished || num_next) {
			finish_and_next(false);
			/* Could have started/finished more. */
			continue;
		}

		if (num_fds == 0)
			break;

		/* You can't tell them all to go to sleep! */
		assert(num_waiting);

		r = poll(pollfds, num_fds, timeout);
		if (r < 0)
			break;

		for (i = 0; i < num_fds && !io_loop_return; i++) {
			struct io_conn *c = (void *)fds[i];
			int events = pollfds[i].revents;

			if (r == 0)
				break;

			if (fds[i]->listener) {
				if (events & POLLIN) {
					accept_conn((void *)c);
					r--;
				}
			} else if (events & (POLLIN|POLLOUT)) {
				r--;
				if (c->duplex) {
					int mask = c->duplex->plan.pollflag;
					if (events & mask) {
						ready(c->duplex);
						events &= ~mask;
						if (!(events&(POLLIN|POLLOUT)))
							continue;
					}
				}
				ready(c);
			} else if (events & POLLHUP) {
				r--;
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
