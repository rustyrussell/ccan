/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_IO_H
#define CCAN_IO_H
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/time/time.h>
#include <stdbool.h>
#include <unistd.h>
#include "io_plan.h"

/**
 * io_new_conn - create a new connection.
 * @fd: the file descriptor.
 * @plan: the first I/O to perform.
 *
 * This creates a connection which owns @fd.  @plan will be called on the
 * next io_loop().
 *
 * Returns NULL on error (and sets errno).
 *
 * Example:
 *	int fd[2];
 *	struct io_conn *conn;
 *
 *	pipe(fd);
 *	// Plan is to close the fd immediately.
 *	conn = io_new_conn(fd[0], io_close());
 *	if (!conn)
 *		exit(1);
 */
#define io_new_conn(fd, plan)				\
	(io_plan_no_debug(), io_new_conn_((fd), (plan)))
struct io_conn *io_new_conn_(int fd, struct io_plan plan);

/**
 * io_set_finish - set finish function on a connection.
 * @conn: the connection.
 * @finish: the function to call when it's closed or fails.
 * @arg: the argument to @finish.
 *
 * @finish will be called when an I/O operation fails, or you call
 * io_close() on the connection.  errno will be set to the value
 * after the failed I/O, or at the call to io_close().  The fd
 * will be closed (unless a duplex) before @finish is called.
 *
 * Example:
 * static void finish(struct io_conn *conn, void *unused)
 * {
 *	// errno is not 0 after success, so this is a bit useless.
 *	printf("Conn %p closed with errno %i\n", conn, errno);
 * }
 * ...
 *	io_set_finish(conn, finish, NULL);
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
 *
 * Example:
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <netdb.h>
 *
 * static void start_conn(int fd, char *msg)
 * {
 *	printf("%s fd %i\n", msg, fd);
 *	close(fd);
 * }
 *
 * // Set up a listening socket, return it.
 * static struct io_listener *do_listen(const char *port)
 * {
 *	struct addrinfo *addrinfo, hints;
 *	int fd, on = 1;
 *
 *	memset(&hints, 0, sizeof(hints));
 *	hints.ai_family = AF_UNSPEC;
 *	hints.ai_socktype = SOCK_STREAM;
 *	hints.ai_flags = AI_PASSIVE;
 *	hints.ai_protocol = 0;
 *
 *	if (getaddrinfo(NULL, port, &hints, &addrinfo) != 0)
 *		return NULL;
 *
 *	fd = socket(addrinfo->ai_family, addrinfo->ai_socktype,
 *		    addrinfo->ai_protocol);
 *	if (fd < 0)
 *		return NULL;
 *
 *	freeaddrinfo(addrinfo);
 *	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
 *	if (bind(fd, addrinfo->ai_addr, addrinfo->ai_addrlen) != 0) {
 *		close(fd);
 *		return NULL;
 *	}
 *	if (listen(fd, 1) != 0) {
 *		close(fd);
 *		return NULL;
 *	}
 *	return io_new_listener(fd, start_conn, (char *)"Got one!");
 * }
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
 *
 * Example:
 * ...
 *	struct io_listener *l = do_listen("8111");
 *	if (l) {
 *		io_loop();
 *		io_close_listener(l);
 *	}
 */
void io_close_listener(struct io_listener *listener);

/**
 * io_write - plan to write data.
 * @data: the data buffer.
 * @len: the length to write.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This creates a plan write out a data buffer.  Once it's all
 * written, the @cb function will be called: on an error, the finish
 * function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 *
 * Example:
 * static void start_conn_with_write(int fd, const char *msg)
 * {
 *	// Write message, then close.
 *	io_new_conn(fd, io_write(msg, strlen(msg), io_close_cb, NULL));
 * }
 */
#define io_write(data, len, cb, arg)					\
	io_debug(io_write_((data), (len),				\
			   typesafe_cb_preargs(struct io_plan, void *,	\
					       (cb), (arg), struct io_conn *), \
			   (arg)))
struct io_plan io_write_(const void *data, size_t len,
			 struct io_plan (*cb)(struct io_conn *, void *),
			 void *arg);

/**
 * io_read - plan to read data.
 * @data: the data buffer.
 * @len: the length to read.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This creates a plan to read data into a buffer.  Once it's all
 * read, the @cb function will be called: on an error, the finish
 * function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 *
 * Example:
 * static void start_conn_with_read(int fd, char msg[12])
 * {
 *	// Read message, then close.
 *	io_new_conn(fd, io_read(msg, 12, io_close_cb, NULL));
 * }
 */
#define io_read(data, len, cb, arg)					\
	io_debug(io_read_((data), (len),				\
			  typesafe_cb_preargs(struct io_plan, void *,	\
					      (cb), (arg), struct io_conn *), \
			  (arg)))
struct io_plan io_read_(void *data, size_t len,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg);


/**
 * io_read_partial - plan to read some data.
 * @data: the data buffer.
 * @len: the maximum length to read, set to the length actually read.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This creates a plan to read data into a buffer.  Once any data is
 * read, @len is updated and the @cb function will be called: on an
 * error, the finish function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 *
 * Example:
 * struct buf {
 *	size_t len;
 *	char buf[12];
 * };
 *
 * static struct io_plan dump_and_close(struct io_conn *conn, struct buf *b)
 * {
 *	printf("Partial read: '%*s'\n", (int)b->len, b->buf);
 *	free(b);
 *	return io_close();
 * }
 *
 * static void start_conn_with_part_read(int fd, void *unused)
 * {
 *	struct buf *b = malloc(sizeof(*b));
 *
 *	// Read message, then dump and close.
 *	b->len = sizeof(b->buf);
 *	io_new_conn(fd, io_read_partial(b->buf, &b->len, dump_and_close, b));
 * }
 */
#define io_read_partial(data, len, cb, arg)				\
	io_debug(io_read_partial_((data), (len),			\
				  typesafe_cb_preargs(struct io_plan, void *, \
						      (cb), (arg),	\
						      struct io_conn *), \
				  (arg)))
struct io_plan io_read_partial_(void *data, size_t *len,
				struct io_plan (*cb)(struct io_conn *, void *),
				void *arg);

/**
 * io_write_partial - plan to write some data.
 * @data: the data buffer.
 * @len: the maximum length to write, set to the length actually written.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This creates a plan to write data from a buffer.   Once any data is
 * written, @len is updated and the @cb function will be called: on an
 * error, the finish function is called instead.
 *
 * Note that the I/O may actually be done immediately.
 *
 * Example:
 * struct buf {
 *	size_t len;
 *	char buf[12];
 * };
 *
 * static struct io_plan show_remainder(struct io_conn *conn, struct buf *b)
 * {
 *	printf("Only wrote: '%*s'\n", (int)b->len, b->buf);
 *	free(b);
 *	return io_close();
 * }
 *
 * static void start_conn_with_part_read(int fd, void *unused)
 * {
 *	struct buf *b = malloc(sizeof(*b));
 *
 *	// Write message, then dump and close.
 *	b->len = sizeof(b->buf);
 *	strcpy(b->buf, "Hello world");
 *	io_new_conn(fd, io_write_partial(b->buf, &b->len, show_remainder, b));
 * }
 */
#define io_write_partial(data, len, cb, arg)				\
	io_debug(io_write_partial_((data), (len),			\
				   typesafe_cb_preargs(struct io_plan, void *, \
						       (cb), (arg),	\
						       struct io_conn *), \
				   (arg)))
struct io_plan io_write_partial_(const void *data, size_t *len,
				 struct io_plan (*cb)(struct io_conn *, void*),
				 void *arg);

/**
 * io_always - plan to immediately call next callback.
 * @cb: function to call.
 * @arg: @cb argument
 *
 * Sometimes it's neater to plan a callback rather than call it directly;
 * for example, if you only need to read data for one path and not another.
 *
 * Example:
 * static void start_conn_with_nothing(int fd)
 * {
 *	// Silly example: close on next time around loop.
 *	io_new_conn(fd, io_always(io_close_cb, NULL));
 * }
 */
#define io_always(cb, arg)						\
	io_debug(io_always_(typesafe_cb_preargs(struct io_plan, void *,	\
						(cb), (arg),		\
						struct io_conn *),	\
			    (arg)))
struct io_plan io_always_(struct io_plan (*cb)(struct io_conn *, void *),
			  void *arg);


/**
 * io_connect - plan to connect to a listening socket.
 * @fd: file descriptor.
 * @addr: where to connect.
 * @cb: function to call once it's done.
 * @arg: @cb argument
 *
 * This initiates a connection, and creates a plan for
 * (asynchronously).  completing it.  Once complete, @len is updated
 * and the @cb function will be called: on an error, the finish
 * function is called instead.
 *
 * Note that the connect may actually be done immediately.
 *
 * Example:
 * #include <sys/types.h>
 * #include <sys/socket.h>
 * #include <netdb.h>
 *
 * // Write, then close socket.
 * static struct io_plan start_write(struct io_conn *conn, void *unused)
 * {
 *	return io_write("hello", 5, io_close_cb, NULL);
 * }
 *
 * ...
 *
 *	int fd;
 *	struct addrinfo *addrinfo;
 *
 *	fd = socket(AF_INET, SOCK_STREAM, 0);
 *	getaddrinfo("localhost", "8111", NULL, &addrinfo);
 *	io_new_conn(fd, io_connect(fd, addrinfo, start_write, NULL));
 */
struct addrinfo;
#define io_connect(fd, addr, cb, arg)					\
	io_debug(io_connect_((fd), (addr),				\
			     typesafe_cb_preargs(struct io_plan, void *, \
						 (cb), (arg),		\
						 struct io_conn *),	\
			     (arg)))
struct io_plan io_connect_(int fd, const struct addrinfo *addr,
			   struct io_plan (*cb)(struct io_conn *, void*),
			   void *arg);

/**
 * io_wait - plan to wait for something.
 * @wait: the address to wait on.
 * @cb: function to call after waiting.
 * @arg: @cb argument
 *
 * This indicates the connection is idle: io_wake() will be called later to
 * restart the connection.
 *
 * Example:
 *	struct io_conn *sleeper;
 *	unsigned int counter = 0;
 *	sleeper = io_new_conn(open("/dev/null", O_RDONLY),
 *			      io_wait(&counter, io_close_cb, NULL));
 *	if (!sleeper)
 *		exit(1);
 */
#define io_wait(wait, cb, arg)						\
	io_debug(io_wait_(wait,						\
			  typesafe_cb_preargs(struct io_plan, void *,	\
					      (cb), (arg),		\
					      struct io_conn *),	\
			  (arg)))

struct io_plan io_wait_(const void *wait,
			struct io_plan (*cb)(struct io_conn *, void *),
			void *arg);

/**
 * io_timeout - set timeout function if the callback doesn't complete.
 * @conn: the current connection.
 * @t: how long until the timeout should be called.
 * @cb: callback to call.
 * @arg: argument to @cb.
 *
 * If the usual next callback is not called for this connection before @ts,
 * this function will be called.  If next callback is called, the timeout
 * is automatically removed.
 *
 * Returns false on allocation failure.  A connection can only have one
 * timeout.
 *
 * Example:
 *	static struct io_plan close_on_timeout(struct io_conn *conn, char *msg)
 *	{
 *		printf("%s\n", msg);
 *		return io_close();
 *	}
 *
 *	...
 *	io_timeout(sleeper, time_from_msec(100),
 *		   close_on_timeout, (char *)"Bye!");
 */
#define io_timeout(conn, ts, fn, arg)					\
	io_timeout_((conn), (ts),					\
		    typesafe_cb_preargs(struct io_plan, void *,		\
					(fn), (arg),			\
					struct io_conn *),		\
		    (arg))
bool io_timeout_(struct io_conn *conn, struct timerel t,
		 struct io_plan (*fn)(struct io_conn *, void *), void *arg);

/**
 * io_duplex - split an fd into two connections.
 * @conn: a connection.
 * @plan: the first I/O function to call.
 *
 * Sometimes you want to be able to simultaneously read and write on a
 * single fd, but io forces a linear call sequence.  The solution is
 * to have two connections for the same fd, and use one for read
 * operations and one for write.
 *
 * You must io_close() both of them to close the fd.
 *
 * Example:
 *	static void setup_read_write(int fd,
 *				     char greet_in[5], const char greet_out[5])
 *	{
 *		struct io_conn *writer, *reader;
 *
 *		// Read their greeting and send ours at the same time.
 *		writer = io_new_conn(fd,
 *				     io_write(greet_out, 5, io_close_cb, NULL));
 *		reader = io_duplex(writer,
 *				     io_read(greet_in, 5, io_close_cb, NULL));
 *		if (!reader || !writer)
 *			exit(1);
 *	}
 */
#define io_duplex(conn, plan)				\
	(io_plan_no_debug(), io_duplex_((conn), (plan)))
struct io_conn *io_duplex_(struct io_conn *conn, struct io_plan plan);

/**
 * io_wake - wake up any connections waiting on @wait
 * @wait: the address to trigger.
 *
 * Example:
 *	unsigned int wait;
 *
 *	io_new_conn(open("/dev/null", O_RDONLY),
 *		   io_wait(&wait, io_close_cb, NULL));
 *
 *	io_wake(&wait);
 */
void io_wake(const void *wait);

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
 *
 * Example:
 *	static struct io_plan fail_on_timeout(struct io_conn *conn, char *msg)
 *	{
 *		return io_break(msg, io_close());
 *	}
 */
#define io_break(ret, plan) (io_plan_no_debug(), io_break_((ret), (plan)))
struct io_plan io_break_(void *ret, struct io_plan plan);

/**
 * io_never - assert if callback is called.
 *
 * Sometimes you want to make it clear that a callback should never happen
 * (eg. for io_break).  This will assert() if called.
 *
 * Example:
 * static struct io_plan break_out(struct io_conn *conn, void *unused)
 * {
 *	// We won't ever return from io_break
 *	return io_break(conn, io_never());
 * }
 */
struct io_plan io_never(void);

/* FIXME: io_recvfrom/io_sendto */

/**
 * io_close - plan to close a connection.
 *
 * On return to io_loop, the connection will be closed.
 *
 * Example:
 * static struct io_plan close_on_timeout(struct io_conn *conn, const char *msg)
 * {
 *	printf("closing: %s\n", msg);
 *	return io_close();
 * }
 */
#define io_close() io_debug(io_close_())
struct io_plan io_close_(void);

/**
 * io_close_cb - helper callback to close a connection.
 * @conn: the connection.
 *
 * This schedules a connection to be closed; designed to be used as
 * a callback function.
 *
 * Example:
 *	#define close_on_timeout io_close_cb
 */
struct io_plan io_close_cb(struct io_conn *, void *unused);

/**
 * io_close_other - close different connection next time around the I/O loop.
 * @conn: the connection to close.
 *
 * This is used to force a different connection to close: no more I/O will
 * happen on @conn, even if it's pending.
 *
 * It's a bug to use this on the current connection!
 *
 * Example:
 * static void stop_connection(struct io_conn *conn)
 * {
 *	printf("forcing stop on connection\n");
 *	io_close_other(conn);
 * }
 */
void io_close_other(struct io_conn *conn);

/**
 * io_loop - process fds until all closed on io_break.
 *
 * This is the core loop; it exits with the io_break() arg, or NULL if
 * all connections and listeners are closed.
 *
 * Example:
 *	io_loop();
 */
void *io_loop(void);

/**
 * io_conn_fd - get the fd from a connection.
 * @conn: the connection.
 *
 * Sometimes useful, eg for getsockname().
 */
int io_conn_fd(const struct io_conn *conn);

/**
 * io_set_alloc - set alloc/realloc/free function for io to use.
 * @allocfn: allocator function
 * @reallocfn: reallocator function, ptr may be NULL, size never 0.
 * @freefn: free function
 *
 * By default io uses malloc/realloc/free, and returns NULL if they fail.
 * You can set your own variants here.
 */
void io_set_alloc(void *(*allocfn)(size_t size),
		  void *(*reallocfn)(void *ptr, size_t size),
		  void (*freefn)(void *ptr));
#endif /* CCAN_IO_H */
