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

/**
 * io_get_plan - get a conn's io_plan for a given direction.
 * @conn: the connection.
 * @dir: IO_IN or IO_OUT.
 *
 * This is how an io helper gets a plan to store into; you must call
 * io_done_plan() when you've initialized it.
 *
 * Example:
 * // Simple helper to read a single char.
 * static int do_readchar(int fd, struct io_plan *plan)
 * {
 *	return read(fd, plan->u1.cp, 1) <= 0 ? -1 : 1;
 * }
 *
 * struct io_plan *io_read_char_(struct io_conn *conn, char *in,
 *				 struct io_plan *(*next)(struct io_conn*,void*),
 *				 void *arg)
 * {
 *	struct io_plan *plan = io_get_plan(conn, IO_IN);
 *
 *	// Store information we need in the plan unions u1 and u2.
 *	plan->u1.cp = in;
 *
 *	return io_set_plan(conn, plan, do_readchar, next, arg);
 * }
 */
struct io_plan *io_get_plan(struct io_conn *conn, enum io_direction dir);

/**
 * io_set_plan - set a conn's io_plan.
 * @conn: the connection.
 * @plan: the plan
 * @io: the IO function to call when the fd is ready.
 * @next: the next callback when @io returns 1.
 * @next_arg: the argument to @next.
 *
 * If @conn has debug set, the io function will be called immediately,
 * so it's important that this be the last thing in your function!
 *
 * See also:
 *	io_get_plan()
 */
struct io_plan *io_set_plan(struct io_conn *conn, struct io_plan *plan,
			    int (*io)(int fd, struct io_plan *plan),
			    struct io_plan *(*next)(struct io_conn *, void *),
			    void *next_arg);
#endif /* CCAN_IO_PLAN_H */
