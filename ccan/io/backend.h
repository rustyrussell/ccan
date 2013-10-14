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
	struct io_plan *(*next)(struct io_conn *, void *arg);
	void (*finish)(struct io_conn *, void *arg);
	void *conn_arg;
};

enum io_result {
	RESULT_AGAIN,
	RESULT_FINISHED,
	RESULT_CLOSE
};

enum io_state {
	IO_IO,
	IO_NEXT, /* eg starting, woken from idle, return from io_break. */
	IO_IDLE,
	IO_FINISHED
};

static inline enum io_state from_ioplan(struct io_plan *op)
{
	return (enum io_state)(long)op;
}

struct io_state_read {
	char *buf;
	size_t len;
};

struct io_state_write {
	const char *buf;
	size_t len;
};

struct io_state_readpart {
	char *buf;
	size_t *lenp;
};

struct io_state_writepart {
	const char *buf;
	size_t *lenp;
};

struct io_timeout {
	struct timer timer;
	struct io_conn *conn;

	struct io_plan *(*next)(struct io_conn *, void *arg);
	void *next_arg;
};

/* One connection per client. */
struct io_conn {
	struct fd fd;

	struct io_plan *(*next)(struct io_conn *, void *arg);
	void *next_arg;

	void (*finish)(struct io_conn *, void *arg);
	void *finish_arg;

	struct io_conn *duplex;
	struct io_timeout *timeout;

	enum io_result (*io)(struct io_conn *conn);

	int pollflag; /* 0, POLLIN or POLLOUT */
	enum io_state state;
	union {
		struct io_state_read read;
		struct io_state_write write;
		struct io_state_readpart readpart;
		struct io_state_writepart writepart;
	} u;
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
void backend_set_state(struct io_conn *conn, struct io_plan *op);
void backend_add_timeout(struct io_conn *conn, struct timespec ts);
void backend_del_timeout(struct io_conn *conn);

struct io_plan *do_ready(struct io_conn *conn);
#endif /* CCAN_IO_BACKEND_H */
