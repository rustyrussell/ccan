/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_H
#define CCAN_IO_H
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/time/time.h>
#include <stdbool.h>
#include <unistd.h>

struct io_conn;

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

/**
 * struct io_plan - returned from a setup function.
 *
 * A plan of what IO to do, when.
 */
struct io_plan {
	int pollflag;
	enum io_state state;
	enum io_result (*io)(struct io_conn *conn);
	struct io_plan (*next)(struct io_conn *, void *arg);
	void *next_arg;

	union {
		struct io_state_read read;
		struct io_state_write write;
		struct io_state_readpart readpart;
		struct io_state_writepart writepart;
	} u;
};

/**
 * io_new_conn - create a new connection.
 * @fd: the file descriptor.
 * @start: the first function to call.
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to both @start and @finish.
 *
 * This creates a connection which owns @fd.  @start will be called on the
 * next return to io_loop(), and @finish will be called when an I/O operation
 * fails, or you call io_close() on the connection.
 *
 * The @start function must call one of the io queueing functions
 * (eg. io_read, io_write) and return the next function to call once
 * that is done using io_next().  The alternative is to call io_close().
 *
 * Returns NULL on error (and sets errno).
 */
#define io_new_conn(fd, start, finish, arg)				\
	io_new_conn_((fd),						\
		     typesafe_cb_preargs(struct io_plan, void *,	\
					 (start), (arg), struct io_conn *), \
		     typesafe_cb_preargs(void, void *, (finish), (arg),	\
					 struct io_conn *),		\
		     (arg))
struct io_conn *io_new_conn_(int fd,
			     struct io_plan (*start)(struct io_conn *, void *),
			     void (*finish)(struct io_conn *, void *),
			     void *arg);

/**
 * io_new_listener - create a new accepting listener.
 * @fd: the file descriptor.
 * @start: the first function to call on new connections.
 * @finish: the function to call when the connection is closed or fails.
 * @arg: the argument to both @start and @finish.
 *
 * When @fd becomes readable, we accept() and turn that fd into a new
 * connection.
 *
 * Returns NULL on error (and sets errno).
 */
#define io_new_listener(fd, start, finish, arg)				\
	io_new_listener_((fd),						\
			 typesafe_cb_preargs(struct io_plan, void *,	\
					     (start), (arg),		\
					     struct io_conn *),		\
			 typesafe_cb_preargs(void, void *, (finish),	\
					     (arg), struct io_conn *),	\
			 (arg))
struct io_listener *io_new_listener_(int fd,
				     struct io_plan (*start)(struct io_conn *,
							      void *arg),
				     void (*finish)(struct io_conn *,
						    void *arg),
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
 * @conn: the current connection.
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
#define io_write(conn, data, len, cb, arg)				\
	io_write_((conn), (data), (len),				\
		  typesafe_cb_preargs(struct io_plan, void *,		\
				      (cb), (arg), struct io_conn *),	\
		  (arg))
struct io_plan io_write_(struct io_conn *conn, const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg);

/**
 * io_read - queue buffer to be read.
 * @conn: the current connection.
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
#define io_read(conn, data, len, cb, arg)				\
	io_read_((conn), (data), (len),					\
		 typesafe_cb_preargs(struct io_plan, void *,		\
				     (cb), (arg), struct io_conn *),	\
		 (arg))
struct io_plan io_read_(struct io_conn *conn, void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg);


/**
 * io_read_partial - queue buffer to be read (partial OK).
 * @conn: the current connection.
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
#define io_read_partial(conn, data, len, cb, arg)			\
	io_read_partial_((conn), (data), (len),				\
			 typesafe_cb_preargs(struct io_plan, void *,	\
					     (cb), (arg), struct io_conn *), \
			 (arg))
struct io_plan io_read_partial_(struct io_conn *conn, void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg);

/**
 * io_write_partial - queue data to be written (partial OK).
 * @conn: the current connection.
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
#define io_write_partial(conn, data, len, cb, arg)			\
	io_write_partial_((conn), (data), (len),			\
			  typesafe_cb_preargs(struct io_plan, void *,	\
					      (cb), (arg), struct io_conn *), \
			  (arg))
struct io_plan io_write_partial_(struct io_conn *conn,
				 const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn *, void*),
				 void *arg);


/**
 * io_idle - explicitly note that this connection will do nothing.
 * @conn: the current connection.
 *
 * This indicates the connection is idle: some other function will
 * later call io_read/io_write etc. (or io_close) on it, in which case
 * it will do that.
 */
struct io_plan io_idle(struct io_conn *conn);

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
 * @start: the first function to call.
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to both @start and @finish.
 *
 * Sometimes you want to be able to simultaneously read and write on a
 * single fd, but io forces a linear call sequence.  The solition is
 * to have two connections for the same fd, and use one for read
 * operations and one for write.
 *
 * You must io_close() both of them to close the fd.
 */
#define io_duplex(conn, start, finish, arg)				\
	io_duplex_((conn),						\
		   typesafe_cb_preargs(struct io_plan, void *,		\
				       (start), (arg), struct io_conn *), \
		   typesafe_cb_preargs(void, void *, (finish), (arg),	\
				       struct io_conn *),		\
		   (arg))

struct io_conn *io_duplex_(struct io_conn *conn,
			   struct io_plan (*start)(struct io_conn *, void *),
			   void (*finish)(struct io_conn *, void *),
			   void *arg);

/**
 * io_wake - wake up and idle connection.
 * @conn: an idle connection.
 * @fn: the next function to call once queued IO is complete.
 * @arg: the argument to @next.
 *
 * This makes @conn run its @next function the next time around the
 * io_loop().
 */
#define io_wake(conn, fn, arg)						\
	io_wake_((conn),						\
		 typesafe_cb_preargs(struct io_plan, void *,		\
				     (fn), (arg), struct io_conn *),	\
		 (arg))
void io_wake_(struct io_conn *conn,
	      struct io_plan (*fn)(struct io_conn *, void *), void *arg);

/**
 * io_break - return from io_loop()
 * @conn: the current connection.
 * @ret: non-NULL value to return from io_loop().
 * @cb: function to call once on return
 * @arg: @cb argument
 *
 * This breaks out of the io_loop.  As soon as the current @next
 * function returns, any io_closed()'d connections will have their
 * finish callbacks called, then io_loop() with return with @ret.
 *
 * If io_loop() is called again, then @cb will be called.
 */
#define io_break(conn, ret, fn, arg)					\
	io_break_((conn), (ret),					\
		  typesafe_cb_preargs(struct io_plan, void *,		\
				      (fn), (arg), struct io_conn *),	\
		  (arg))
struct io_plan io_break_(struct io_conn *conn, void *ret,
			 struct io_plan (*fn)(struct io_conn *, void *),
			 void *arg);

/* FIXME: io_recvfrom/io_sendto */

/**
 * io_close - terminate a connection.
 * @conn: any connection.
 *
 * The schedules a connection to be closed.  It can be done on any
 * connection, whether it has I/O queued or not (though that I/O may
 * be performed first).
 *
 * It's common to 'return io_close(...)' from a @next function, but
 * io_close can also be used as an argument to io_next().
 */
struct io_plan io_close(struct io_conn *, void *unused);

/**
 * io_loop - process fds until all closed on io_break.
 *
 * This is the core loop; it exits with the io_break() arg, or NULL if
 * all connections and listeners are closed.
 */
void *io_loop(void);
#endif /* CCAN_IO_H */
