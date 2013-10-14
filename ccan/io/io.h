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
 * @plan: the first I/O function.
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to @finish.
 *
 * This creates a connection which owns @fd.  @plan will be called on the
 * next io_loop(), and @finish will be called when an I/O operation
 * fails, or you call io_close() on the connection.
 *
 * Returns NULL on error (and sets errno).
 */
#define io_new_conn(fd, plan, finish, arg)				\
	io_new_conn_((fd), (plan),					\
		     typesafe_cb_preargs(void, void *, (finish), (arg),	\
					 struct io_conn *),		\
		     (arg))
struct io_conn *io_new_conn_(int fd,
			     struct io_plan plan,
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
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to @finish.
 *
 * Sometimes you want to be able to simultaneously read and write on a
 * single fd, but io forces a linear call sequence.  The solition is
 * to have two connections for the same fd, and use one for read
 * operations and one for write.
 *
 * You must io_close() both of them to close the fd.
 */
#define io_duplex(conn, plan, finish, arg)				\
	io_duplex_((conn), (plan),					\
		   typesafe_cb_preargs(void, void *, (finish), (arg),	\
				       struct io_conn *),		\
		   (arg))

struct io_conn *io_duplex_(struct io_conn *conn,
			   struct io_plan plan,
			   void (*finish)(struct io_conn *, void *),
			   void *arg);

/**
 * io_wake - wake up an idle connection.
 * @conn: an idle connection.
 * @plan: the next I/O function for @conn.
 *
 * This makes @conn do I/O the next time around the io_loop().
 */
void io_wake(struct io_conn *conn, struct io_plan plan);

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
struct io_plan io_break(void *ret, struct io_plan plan);

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
