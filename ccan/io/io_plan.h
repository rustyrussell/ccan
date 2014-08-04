/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_PLAN_H
#define CCAN_IO_PLAN_H
struct io_conn;

/**
 * union io_plan_arg - scratch space for struct io_plan read/write fns.
 */
union io_plan_arg {
	char *cp;
	void *vp;
	const void *const_vp;
	size_t s;
	char c[sizeof(size_t)];
};

enum io_plan_status {
	/* As before calling next function. */
	IO_UNSET,
	/* Normal. */
	IO_POLLING,
	/* Waiting for io_wake */
	IO_WAITING,
	/* Always do this. */
	IO_ALWAYS,
	/* Closing (both plans will be the same). */
	IO_CLOSING
};

enum io_direction {
	IO_IN,
	IO_OUT
};

/**
 * struct io_plan - one half of I/O to do
 * @status: the status of this plan.
 * @io: function to call when fd becomes read/writable, returns 0 to be
 *      called again, 1 if it's finished, and -1 on error (fd will be closed)
 * @next: the next function which is called if io returns 1.
 * @next_arg: the argument to @next
 * @u1, @u2: scratch space for @io.
 */
struct io_plan {
	enum io_plan_status status;

	int (*io)(int fd, struct io_plan *plan);

	struct io_plan *(*next)(struct io_conn *, void *arg);
	void *next_arg;

	union io_plan_arg u1, u2;
};

/* Helper to get a conn's io_plan. */
struct io_plan *io_get_plan(struct io_conn *conn, enum io_direction dir);

#endif /* CCAN_IO_PLAN_H */
