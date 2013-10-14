/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_BACKEND_H
#define CCAN_IO_BACKEND_H
#include <stdbool.h>
#include <ccan/timer/timer.h>

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

bool add_listener(struct io_listener *l);
bool add_conn(struct io_conn *c);
bool add_duplex(struct io_conn *c);
void del_listener(struct io_listener *l);
void backend_wakeup(struct io_conn *conn);
void backend_add_timeout(struct io_conn *conn, struct timespec ts);
void backend_del_timeout(struct io_conn *conn);

struct io_plan do_ready(struct io_conn *conn);
#endif /* CCAN_IO_BACKEND_H */
