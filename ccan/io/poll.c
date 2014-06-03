/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#include "io.h"
#include "backend.h"
#include <assert.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <limits.h>
#include <errno.h>

static size_t num_fds = 0, max_fds = 0, num_closing = 0, num_waiting = 0;
static bool some_always = false;
static struct pollfd *pollfds = NULL;
static struct fd **fds = NULL;
static struct timers timeouts;
#ifdef DEBUG
static unsigned int io_loop_level;
static struct io_conn *free_later;
static void io_loop_enter(void)
{
	io_loop_level++;
}
static void io_loop_exit(void)
{
	io_loop_level--;
	if (io_loop_level == 0) {
		/* Delayed free. */
		while (free_later) {
			struct io_conn *c = free_later;
			free_later = c->finish_arg;
			io_alloc.free(c);
		}
	}
}
static void free_conn(struct io_conn *conn)
{
	/* Only free on final exit: chain via finish. */
	if (io_loop_level > 1) {
		struct io_conn *c;
		for (c = free_later; c; c = c->finish_arg)
			assert(c != conn);
		conn->finish_arg = free_later;
		free_later = conn;
	} else
		io_alloc.free(conn);
}
#else
static void io_loop_enter(void)
{
}
static void io_loop_exit(void)
{
}
static void free_conn(struct io_conn *conn)
{
	io_alloc.free(conn);
}
#endif

static bool add_fd(struct fd *fd, short events)
{
	if (num_fds + 1 > max_fds) {
		struct pollfd *newpollfds;
		struct fd **newfds;
		size_t num = max_fds ? max_fds * 2 : 8;

		newpollfds = io_alloc.realloc(pollfds, sizeof(*newpollfds)*num);
		if (!newpollfds)
			return false;
		pollfds = newpollfds;
		newfds = io_alloc.realloc(fds, sizeof(*newfds) * num);
		if (!newfds)
			return false;
		fds = newfds;
		max_fds = num;
	}

	pollfds[num_fds].events = events;
	/* In case it's idle. */
	if (!events)
		pollfds[num_fds].fd = -fd->fd;
	else
		pollfds[num_fds].fd = fd->fd;
	pollfds[num_fds].revents = 0; /* In case we're iterating now */
	fds[num_fds] = fd;
	fd->backend_info = num_fds;
	num_fds++;
	if (events)
		num_waiting++;

	return true;
}

static void del_fd(struct fd *fd)
{
	size_t n = fd->backend_info;

	assert(n != -1);
	assert(n < num_fds);
	if (pollfds[n].events)
		num_waiting--;
	if (n != num_fds - 1) {
		/* Move last one over us. */
		pollfds[n] = pollfds[num_fds-1];
		fds[n] = fds[num_fds-1];
		assert(fds[n]->backend_info == num_fds-1);
		fds[n]->backend_info = n;
		/* If that happens to be a duplex, move that too. */
		if (!fds[n]->listener) {
			struct io_conn *c = (void *)fds[n];
			if (c->duplex) {
				assert(c->duplex->fd.backend_info == num_fds-1);
				c->duplex->fd.backend_info = n;
			}
		}
	} else if (num_fds == 1) {
		/* Free everything when no more fds. */
		io_alloc.free(pollfds);
		io_alloc.free(fds);
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
	return true;
}

void backend_plan_changed(struct io_conn *conn)
{
	struct pollfd *pfd;

	/* This can happen with debugging and delayed free... */
	if (conn->fd.backend_info == -1)
		return;

	pfd = &pollfds[conn->fd.backend_info];

	if (pfd->events)
		num_waiting--;

	pfd->events = conn->plan.pollflag & (POLLIN|POLLOUT);
	if (conn->duplex) {
		int mask = conn->duplex->plan.pollflag & (POLLIN|POLLOUT);
		/* You can't *both* read/write. */
		assert(!mask || pfd->events != mask);
		pfd->events |= mask;
	}
	if (pfd->events) {
		num_waiting++;
		pfd->fd = conn->fd.fd;
	} else
		pfd->fd = -conn->fd.fd;

	if (!conn->plan.next)
		num_closing++;

	if (conn->plan.pollflag == POLLALWAYS)
		some_always = true;
}

void backend_wait_changed(const void *wait)
{
	unsigned int i;

	for (i = 0; i < num_fds; i++) {
		struct io_conn *c, *duplex;

		/* Ignore listeners */
		if (fds[i]->listener)
			continue;
		c = (void *)fds[i];
		for (duplex = c->duplex; c; c = duplex, duplex = NULL) {
			/* Ignore closing. */
			if (!c->plan.next)
				continue;
			/* Not idle? */
			if (c->plan.io)
				continue;
			/* Waiting on something else? */
			if (c->plan.u1.const_vp != wait)
				continue;
			/* Make it do the next thing. */
			c->plan = io_always_(c->plan.next, c->plan.next_arg);
			backend_plan_changed(c);
		}
	}
}

bool add_conn(struct io_conn *c)
{
	if (!add_fd(&c->fd, c->plan.pollflag & (POLLIN|POLLOUT)))
		return false;
	/* Immediate close is allowed. */
	if (!c->plan.next)
		num_closing++;
	if (c->plan.pollflag == POLLALWAYS)
		some_always = true;
	return true;
}

bool add_duplex(struct io_conn *c)
{
	c->fd.backend_info = c->duplex->fd.backend_info;
	backend_plan_changed(c);
	return true;
}

void backend_del_conn(struct io_conn *conn)
{
	if (timeout_active(conn))
		backend_del_timeout(conn);
	io_alloc.free(conn->timeout);
	if (conn->duplex) {
		/* In case fds[] pointed to the other one. */
		assert(conn->duplex->fd.backend_info == conn->fd.backend_info);
		fds[conn->fd.backend_info] = &conn->duplex->fd;
		conn->duplex->duplex = NULL;
		conn->fd.backend_info = -1;
	} else
		del_fd(&conn->fd);
	num_closing--;
	if (conn->finish) {
		/* Saved by io_close */
		errno = conn->plan.u1.s;
		conn->finish(conn, conn->finish_arg);
	}
	free_conn(conn);
}

void del_listener(struct io_listener *l)
{
	del_fd(&l->fd);
}

static void set_plan(struct io_conn *conn, struct io_plan plan)
{
	conn->plan = plan;
	backend_plan_changed(conn);
}

static void accept_conn(struct io_listener *l)
{
	int fd = accept(l->fd.fd, NULL, NULL);

	/* FIXME: What to do here? */
	if (fd < 0)
		return;
	l->init(fd, l->arg);
}

/* It's OK to miss some, as long as we make progress. */
static bool finish_conns(struct io_conn **ready)
{
	unsigned int i;

	for (i = 0; !io_loop_return && i < num_fds; i++) {
		struct io_conn *c, *duplex;

		if (!num_closing)
			break;

		if (fds[i]->listener)
			continue;
		c = (void *)fds[i];
		for (duplex = c->duplex; c; c = duplex, duplex = NULL) {
			if (!c->plan.next) {
				if (doing_debug_on(c) && ready) {
					*ready = c;
					return true;
				}
				backend_del_conn(c);
				i--;
			}
		}
	}
	return false;
}

void backend_add_timeout(struct io_conn *conn, struct timerel duration)
{
	if (!timeouts.base)
		timers_init(&timeouts, time_now());
	timer_add(&timeouts, &conn->timeout->timer,
		  timeabs_add(time_now(), duration));
	conn->timeout->conn = conn;
}

void backend_del_timeout(struct io_conn *conn)
{
	assert(conn->timeout->conn == conn);
	timer_del(&timeouts, &conn->timeout->timer);
	conn->timeout->conn = NULL;
}

static void handle_always(void)
{
	int i;

	some_always = false;

	for (i = 0; i < num_fds && !io_loop_return; i++) {
		struct io_conn *c = (void *)fds[i];

		if (fds[i]->listener)
			continue;

		if (c->plan.pollflag == POLLALWAYS)
			io_ready(c);

		if (c->duplex && c->duplex->plan.pollflag == POLLALWAYS)
			io_ready(c->duplex);
	}
}

/* This is the main loop. */
void *do_io_loop(struct io_conn **ready)
{
	void *ret;

	io_loop_enter();

	while (!io_loop_return) {
		int i, r, timeout = INT_MAX;
		struct timeabs now;
		bool some_timeouts = false;

		if (timeouts.base) {
			struct timeabs first;
			struct list_head expired;
			struct io_timeout *t;

			now = time_now();

			/* Call functions for expired timers. */
			timers_expire(&timeouts, now, &expired);
			while ((t = list_pop(&expired, struct io_timeout, timer.list))) {
				struct io_conn *conn = t->conn;
				/* Clear, in case timer re-adds */
				t->conn = NULL;
				set_current(conn);
				set_plan(conn, t->next(conn, t->next_arg));
				some_timeouts = true;
			}

			/* Now figure out how long to wait for the next one. */
			if (timer_earliest(&timeouts, &first)) {
				uint64_t f = time_to_msec(time_between(first, now));
				if (f < INT_MAX)
					timeout = f;
			}
		}

		if (num_closing) {
			/* If this finishes a debugging con, return now. */
			if (finish_conns(ready))
				return NULL;
			/* Could have started/finished more. */
			continue;
		}

		/* debug can recurse on io_loop; anything can change. */
		if (doing_debug() && some_timeouts)
			continue;

		if (some_always) {
			handle_always();
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
						if (doing_debug_on(c->duplex)
							&& ready) {
							*ready = c->duplex;
							return NULL;
						}
						io_ready(c->duplex);
						events &= ~mask;
						/* debug can recurse;
						 * anything can change. */
						if (doing_debug())
							break;

						/* If no events, or it closed
						 * the duplex, continue. */
						if (!(events&(POLLIN|POLLOUT))
						    || !c->plan.next)
							continue;
					}
				}
				if (doing_debug_on(c) && ready) {
					*ready = c;
					return NULL;
				}
				io_ready(c);
				/* debug can recurse; anything can change. */
				if (doing_debug())
					break;
			} else if (events & (POLLHUP|POLLNVAL|POLLERR)) {
				r--;
				set_current(c);
				errno = EBADF;
				set_plan(c, io_close());
				if (c->duplex) {
					set_current(c->duplex);
					set_plan(c->duplex, io_close());
				}
			}
		}
	}

	while (num_closing && !io_loop_return) {
		if (finish_conns(ready))
			return NULL;
	}

	ret = io_loop_return;
	io_loop_return = NULL;

	io_loop_exit();
	return ret;
}

void *io_loop(void)
{
	return do_io_loop(NULL);
}
