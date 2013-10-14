/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_IO_BACKEND_H
#define CCAN_IO_BACKEND_H
#include <stdbool.h>

struct fd {
	int fd;
	bool listener;
	size_t backend_info;

	struct io_op *(*next)(struct io_conn *, void *arg);
	void *next_arg;

	void (*finish)(struct io_conn *, void *arg);
	void *finish_arg;
};


/* Listeners create connections. */
struct io_listener {
	struct fd fd;
};

enum io_state {
	NEXT, /* eg starting, woken from idle, return from io_break. */
	READ,
	WRITE,
	READPART,
	WRITEPART,
	IDLE,
	FINISHED,
	PROCESSING /* We expect them to change this now. */
};

static inline enum io_state from_ioop(struct io_op *op)
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

/* One connection per client. */
struct io_conn {
	struct fd fd;

	enum io_state state;
	union {
		struct io_state_read read;
		struct io_state_write write;
		struct io_state_readpart readpart;
		struct io_state_writepart writepart;
	} u;
};

extern void *io_loop_return;

bool add_listener(struct io_listener *l);
bool add_conn(struct io_conn *c);
void del_listener(struct io_listener *l);
void backend_set_state(struct io_conn *conn, struct io_op *op);

struct io_op *do_writeable(struct io_conn *conn);
struct io_op *do_readable(struct io_conn *conn);
#endif /* CCAN_IO_BACKEND_H */
