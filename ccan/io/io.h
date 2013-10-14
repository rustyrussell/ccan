/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_H
#define CCAN_IO_H
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/time/time.h>
#include <stdbool.h>
#include <unistd.h>

struct io_conn;

/**
 * struct io_plan - returned from a setup function.
 *
 * A plan of what IO to do, when.
 */
struct io_plan {
	int pollflag;
	/* Only NULL if idle. */
	int (*io)(int fd, struct io_plan *plan);
	/* Only NULL if closing. */
	struct io_plan (*next)(struct io_conn *, void *arg);
	void *next_arg;

	union {
		struct {
			char *buf;
			size_t len;
		} read;
		struct {
			const char *buf;
			size_t len;
		} write;
		struct {
			char *buf;
			size_t *lenp;
		} readpart;
		struct {
			const char *buf;
			size_t *lenp;
		} writepart;
		struct {
			int saved_errno;
		} close;
		struct {
			void *p;
			size_t len;
		} ptr_len;
		struct {
			void *p1;
			void *p2;
		} ptr_ptr;
		struct {
			size_t len1;
			size_t len2;
		} len_len;
	} u;
};

#ifdef DEBUG
extern bool io_plan_for_other;
extern bool (*io_debug)(struct io_conn *conn);
#define io_plan_other() ((io_plan_for_other = true))
void io_plan_debug(struct io_plan *plan);
#else
#define io_plan_other() (void)0
static inline void io_plan_debug(struct io_plan *plan) { }
#endif

/**
 * io_new_conn - create a new connection.
 * @fd: the file descriptor.
 * @plan: the first I/O function.
 *
 * This creates a connection which owns @fd.  @plan will be called on the
 * next io_loop().
 *
 * Returns NULL on error (and sets errno).
 */
#define io_new_conn(fd, plan)				\
	(io_plan_other(), io_new_conn_((fd), (plan)))
struct io_conn *io_new_conn_(int fd, struct io_plan plan);

/**
 * io_set_finish - set finish function on a connection.
 * @conn: the connection.
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to @finish.
 *
 * @finish will be called when an I/O operation fails, or you call
 * io_close() on the connection.  errno will be set to the value
 * after the failed I/O, or at the call to io_close().
 */
#define io_set_finish(conn, finish, arg)				\
	io_set_finish_((conn),						\
		       typesafe_cb_preargs(void, void *,		\
					   (finish), (arg),		\
					   struct io_conn *),		\
		       (arg))
void io_set_finish_(struct io_conn *conn,
		    void (*finish)(struct io_conn *, void *),
		    void *arg);

/**
 * io_new_listener - create a new accepting listener.
 * @fd: the file descriptor.
 * @init: the function to call for a new connection
 * @arg: the argument to @init.
 *
 * When @fd becomes readable, we accept() and pass that fd to init().
 *
 * Returns NULL on error (and sets errno).
 */
#define io_new_listener(fd, init, arg)					\
	io_new_listener_((fd),						\
			 typesafe_cb_preargs(void, void *,		\
					     (init), (arg),		\
					     int fd),			\
			 (arg))
struct io_listener *io_new_listener_(int fd,
				     void (*init)(int fd, void *arg),
				     void *arg);

/**
 * io_close_listener - delete a listener.
 * @listener: the listener returned from io_new_listener.
 *
 * This closes the fd and frees @listener.
 */
void io_close_listener(struct io_listener *listener);

/**
 * io_write - queue data to be written.
 * @data: the data buffer.
 * @len: the length to write.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This will queue the data buffer for writing.  Once it's all
 * written, the @cb function will be called: on an error, the finish
 * function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 */
#define io_write(data, len, cb, arg)					\
	io_write_((data), (len),					\
		  typesafe_cb_preargs(struct io_plan, void *,		\
				      (cb), (arg), struct io_conn *),	\
		  (arg))
struct io_plan io_write_(const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg);

/**
 * io_read - queue buffer to be read.
 * @data: the data buffer.
 * @len: the length to read.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This will queue the data buffer for reading.  Once it's all read,
 * the @cb function will be called: on an error, the finish function
 * is called instead.
 *
 * Note that the I/O may actually be done immediately.
 */
#define io_read(data, len, cb, arg)					\
	io_read_((data), (len),						\
		 typesafe_cb_preargs(struct io_plan, void *,		\
				     (cb), (arg), struct io_conn *),	\
		 (arg))
struct io_plan io_read_(void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg);


/**
 * io_read_partial - queue buffer to be read (partial OK).
 * @data: the data buffer.
 * @len: the maximum length to read, set to the length actually read.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This will queue the data buffer for reading.  Once any data is
 * read, @len is updated and the @cb function will be called: on an
 * error, the finish function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 */
#define io_read_partial(data, len, cb, arg)				\
	io_read_partial_((data), (len),					\
			 typesafe_cb_preargs(struct io_plan, void *,	\
					     (cb), (arg), struct io_conn *), \
			 (arg))
struct io_plan io_read_partial_(void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg);

/**
 * io_write_partial - queue data to be written (partial OK).
 * @data: the data buffer.
 * @len: the maximum length to write, set to the length actually written.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This will queue the data buffer for writing.  Once any data is
 * written, @len is updated and the @cb function will be called: on an
 * error, the finish function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 */
#define io_write_partial(data, len, cb, arg)				\
	io_write_partial_((data), (len),				\
			  typesafe_cb_preargs(struct io_plan, void *,	\
					      (cb), (arg), struct io_conn *), \
			  (arg))
struct io_plan io_write_partial_(const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn *, void*),
				 void *arg);


/**
 * io_idle - explicitly note that this connection will do nothing.
 *
 * This indicates the connection is idle: some other function will
 * later call io_read/io_write etc. (or io_close) on it, in which case
 * it will do that.
 */
struct io_plan io_idle(void);

/**
 * io_timeout - set timeout function if the callback doesn't fire.
 * @conn: the current connection.
 * @ts: how long until the timeout should be called.
 * @cb to call.
 * @arg: argument to @cb.
 *
 * If the usual next callback is not called for this connection before @ts,
 * this function will be called.  If next callback is called, the timeout
 * is automatically removed.
 *
 * Returns false on allocation failure.  A connection can only have one
 * timeout.
 */
#define io_timeout(conn, ts, fn, arg)					\
	io_timeout_((conn), (ts),					\
		    typesafe_cb_preargs(struct io_plan, void *,		\
					(fn), (arg),			\
					struct io_conn *),		\
		    (arg))
bool io_timeout_(struct io_conn *conn, struct timespec ts,
		 struct io_plan (*fn)(struct io_conn *, void *), void *arg);

/**
 * io_duplex - split an fd into two connections.
 * @conn: a connection.
 * @plan: the first I/O function to call.
 *
 * Sometimes you want to be able to simultaneously read and write on a
 * single fd, but io forces a linear call sequence.  The solition is
 * to have two connections for the same fd, and use one for read
 * operations and one for write.
 *
 * You must io_close() both of them to close the fd.
 */
#define io_duplex(conn, plan)				\
	(io_plan_other(), io_duplex_((conn), (plan)))

struct io_conn *io_duplex_(struct io_conn *conn, struct io_plan plan);

/**
 * io_wake - wake up an idle connection.
 * @conn: an idle connection.
 * @plan: the next I/O function for @conn.
 *
 * This makes @conn do I/O the next time around the io_loop().
 */
#define io_wake(conn, plan) (io_plan_other(), io_wake_((conn), (plan)))
void io_wake_(struct io_conn *conn, struct io_plan plan);

/**
 * io_break - return from io_loop()
 * @ret: non-NULL value to return from io_loop().
 * @plan: I/O to perform on return (if any)
 *
 * This breaks out of the io_loop.  As soon as the current @next
 * function returns, any io_closed()'d connections will have their
 * finish callbacks called, then io_loop() with return with @ret.
 *
 * If io_loop() is called again, then @plan will be carried out.
 */
#define io_break(ret, plan) (io_plan_other(), io_break_((ret), (plan)))
struct io_plan io_break_(void *ret, struct io_plan plan);

/* FIXME: io_recvfrom/io_sendto */

/**
 * io_close - plan to close a connection.
 *
 * On return to io_loop, the connection will be closed.
 */
struct io_plan io_close(void);

/**
 * io_close_cb - helper callback to close a connection.
 * @conn: the connection.
 *
 * This schedules a connection to be closed; designed to be used as
 * a callback function.
 */
struct io_plan io_close_cb(struct io_conn *, void *unused);

/**
 * io_loop - process fds until all closed on io_break.
 *
 * This is the core loop; it exits with the io_break() arg, or NULL if
 * all connections and listeners are closed.
 */
void *io_loop(void);
#endif /* CCAN_IO_H */
