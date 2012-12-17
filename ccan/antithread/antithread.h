/* Licensed under GPLv3+ - see LICENSE file for details */
#ifndef ANTITHREAD_H
#define ANTITHREAD_H
#include <ccan/typesafe_cb/typesafe_cb.h>
#include <ccan/tal/tal.h>

struct at_pool;
struct at_child;
struct at_parent;

/**
 * at_new_pool - allocate a new sharable pool.
 * @size: number of bytes in the pool.
 *
 * The initial process usually creates the pool, and they all share
 * it.  Note that the returned object is within the pool, and is its
 * own parent: it is not linked to the (non-shared) NULL node, as that
 * connection would be incorrect for the children.
 *
 * If you pass the returned value as a @ctx to tal, the resulting
 * object will be allocated from this pool, thus visible to everyone.
 */
struct at_pool *at_new_pool(size_t size);

/**
 * at_run - fork() a child process.
 * @pool: the pool to share.
 * @fn: where to start the new process: void *@fn(struct at_pool *, @arg)
 * @arg: the argument to pass to @fn (must be the type @fn expects).
 *
 * If @fn returns, that return value (often NULL) is passed back to the
 * parent via at_tell_parent().
 *
 * The struct at_parent handed to @fn is a useful context for the
 * child to tal shared memory from.
 */
#define at_run(pool, fn, arg)						\
	_at_run(pool,							\
		typesafe_cb_preargs(void *, void *, (fn), (arg),	\
				    struct at_parent *),			\
		(arg))

/**
 * at_spawn - fork() and execvp() a child process.
 * @pool: the pool to share.
 * @arg: the argument to pass it.
 * @cmdline: the command to execute.
 *
 * The new process is executed with an extra argument; it is expected
 * to collect the pool and arg with at_get_parent().
 */
struct at_child *at_spawn(struct at_pool *pool, void *arg, char *cmdline[]);

/**
 * at_get_parent - unpack the pool and parent from an at_spawn()ed child.
 * @argc: the command-line argument counter.
 * @argv: the command-line arguments.
 * @arg: the unpacked arg to at_spawn().
 *
 * This returns the at_pool (and @arg) for the process, as passed to
 * at_spawn() in the parent.   It modifies the command-line arguments
 * to erase the extra argument inserted by at_spawn().
 */
struct at_parent *at_get_parent(int *argc, char *argv[], void **arg);

/**
 * at_read_child - read a message from this child process.
 * @at: the process created via at_run or at_spawn.
 *
 * Returns NULL if the child died.  It will block reading the fd.
 */
void *at_read_child(const struct at_child *at);

/**
 * at_tell_child - send a message to this child process.
 * @at: the process created via at_run or at_spawn.
 * @ptr: the message to send.
 *
 * Returns false if the child died (assuming you're ignoring SIGPIPE).
 * It may block if the pipe is completely full.
 */
bool at_tell_child(const struct at_child *at, const void *ptr);

/**
 * at_child_rfd - the fd to get messages from this process
 * @at: the process created via at_run or at_spawn.
 *
 * Useful for writing poll()/select() loops in the parent.
 */
int at_child_rfd(struct at_child *at);

/**
 * at_tell_parent - send a message to our parent.
 * @parent: the parent.
 * @ptr: the pointer to send.
 *
 * Returns false if the parent died (assuming you're ignoring SIGPIPE).
 * It may block if the pipe is completely full.
 */
bool at_tell_parent(struct at_parent *parent, const void *ptr);

/**
 * at_read_parent - receive a message from our parent.
 * @parent: the parent.
 *
 * This will return NULL if the parent died.  It will block reading
 * the fd.
 */
void *at_read_parent(struct at_parent *parent);

/**
 * at_parent_rfd - the fd to get messages from the parent
 * @parent: the parent.
 *
 * Useful for writing poll()/select() loops in the parent.
 */
int at_parent_rfd(struct at_parent *parent);

/**
 * at_lock - lock an object in the pool.
 * @obj: the object to lock.
 *
 * This can be used for coherence between processes.
 */
void at_lock(const tal_t *obj);

/**
 * at_unlock - unlock an object in the pool.
 * @obj: the object to lock.
 */
void at_unlock(const tal_t *obj);

bool at_check_pool(struct at_pool *pool, const char *abortstr);

/* Internal function */
struct at_child *_at_run(struct at_pool *pool,
			 void *(*fn)(struct at_parent *, void *arg),
			 void *arg);

#endif /* ANTITHREAD_H */
