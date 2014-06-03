/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_BACKEND_H
#define CCAN_IO_BACKEND_H
#include <stdbool.h>
#include <ccan/timer/timer.h>
#include <poll.h>

/* A setting for actions to always run (eg. zero-length reads). */
#define POLLALWAYS (((POLLIN|POLLOUT) + 1) & ~((POLLIN|POLLOUT)))

struct io_alloc {
	void *(*alloc)(size_t size);
	void *(*realloc)(void *ptr, size_t size);
	void (*free)(void *ptr);
};
extern struct io_alloc io_alloc;

struct fd {
	int fd;
	bool listener;
	size_t backend_info;
};

/* Listeners create connections. */
struct io_listener {
	struct fd fd;

	/* These are for connections we create. */
	void (*init)(int fd, void *arg);
	void *arg;
};

struct io_timeout {
	struct timer timer;
	struct io_conn *conn;

	struct io_plan (*next)(struct io_conn *, void *arg);
	void *next_arg;
};

/* One connection per client. */
struct io_conn {
	struct fd fd;

	void (*finish)(struct io_conn *, void *arg);
	void *finish_arg;

	struct io_conn *duplex;
	struct io_timeout *timeout;

	struct io_plan plan;
};

static inline bool timeout_active(const struct io_conn *conn)
{
	return conn->timeout && conn->timeout->conn;
}

extern void *io_loop_return;

#ifdef DEBUG
extern struct io_conn *current;
static inline void set_current(struct io_conn *conn)
{
	current = conn;
}
static inline bool doing_debug_on(struct io_conn *conn)
{
	return io_debug_conn && io_debug_conn(conn);
}
static inline bool doing_debug(void)
{
	return io_debug_conn;
}
#else
static inline void set_current(struct io_conn *conn)
{
}
static inline bool doing_debug_on(struct io_conn *conn)
{
	return false;
}
static inline bool doing_debug(void)
{
	return false;
}
#endif

bool add_listener(struct io_listener *l);
bool add_conn(struct io_conn *c);
bool add_duplex(struct io_conn *c);
void del_listener(struct io_listener *l);
void backend_plan_changed(struct io_conn *conn);
void backend_wait_changed(const void *wait);
void backend_add_timeout(struct io_conn *conn, struct timerel duration);
void backend_del_timeout(struct io_conn *conn);
void backend_del_conn(struct io_conn *conn);

void io_ready(struct io_conn *conn);
void *do_io_loop(struct io_conn **ready);
#endif /* CCAN_IO_BACKEND_H */
