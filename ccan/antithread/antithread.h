/* Licensed under GPLv3+ - see LICENSE file for details */
#ifndef ANTITHREAD_H
#define ANTITHREAD_H
#include <ccan/typesafe_cb/typesafe_cb.h>

struct at_pool;
struct athread;

/* Operations for the parent. */

/* Create a new sharable pool. */
struct at_pool *at_pool(unsigned long size);

/* Talloc off this to allocate from within the pool. */
const void *at_pool_ctx(struct at_pool *atp);

/* Creating an antithread via fork().  Returned athread is child of pool. */
#define at_run(pool, fn, arg)						\
	_at_run(pool,							\
		typesafe_cb_preargs(void *, void *, (fn), (arg), struct at_pool *), \
		(arg))

/* Fork and execvp, with added arguments for child to grab.
 * Returned athread is child of pool. */
struct athread *at_spawn(struct at_pool *pool, void *arg, char *cmdline[]);

/* The fd to poll on */
int at_fd(struct athread *at);

/* What's the antithread saying?  Blocks if fd not ready. */
void *at_read(struct athread *at);

/* Say something to a child (async). */
void at_tell(struct athread *at, const void *status);

/* Operations for the children */
/* For child to grab arguments from command line (removes them) */
struct at_pool *at_get_pool(int *argc, char *argv[], void **arg);

/* Say something to our parent (async). */
void at_tell_parent(struct at_pool *pool, const void *status);

/* What's the parent saying?  Blocks if fd not ready. */
void *at_read_parent(struct at_pool *pool);

/* The fd to poll on */
int at_parent_fd(struct at_pool *pool);

/* Locking: any talloc pointer. */
void at_lock(void *obj);
void at_unlock(void *obj);

void at_lock_all(struct at_pool *pool);
void at_unlock_all(struct at_pool *pool);

/* Internal function */
struct athread *_at_run(struct at_pool *pool,
			void *(*fn)(struct at_pool *, void *arg),
			void *arg);

#endif /* ANTITHREAD_H */
