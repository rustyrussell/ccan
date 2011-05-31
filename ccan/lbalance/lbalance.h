#ifndef CCAN_LBALANCE_H
#define CCAN_LBALANCE_H
#include "config.h"

struct lbalance;
struct lbalance_task;
struct timeval;
struct rusage;

/**
 * lbalance_new - initialize a load balancing structure.
 */
struct lbalance *lbalance_new(void);

/**
 * lbalance_task_new - mark the starting of a new task.
 * @lbalance: the load balancer from lbalance_new.
 */
struct lbalance_task *lbalance_task_new(struct lbalance *lbalance);

/**
 * lbalance_task_free - mark the completion of a task.
 * @task: the lbalance_task from lbalance_task_new, which will be freed.
 * @usage: the resource usage for that task (or NULL).
 *
 * If @usage is NULL, you must have already wait()ed for the child so
 * that lbalance_task_free() can derive it from the difference in
 * getrusage() for the child processes.
 *
 * Otherwise, lbalance_task_free() is a noop, which is useful for failure
 * paths.
 */
void lbalance_task_free(struct lbalance_task *task,
			const struct rusage *usage);

/**
 * lbalance_target - how many tasks in parallel are recommended?
 * @lbalance: the load balancer from lbalance_new.
 *
 * Normally you keep creating tasks until this limit is reached.  It's
 * updated by stats from lbalance_task_free.
 */
unsigned lbalance_target(struct lbalance *lbalance);

/**
 * lbalance_free - free a load balancing structure.
 * @lbalance: the load balancer from lbalance_new.
 *
 * Also frees any tasks still attached.
 */
void lbalance_free(struct lbalance *lbalance);
#endif /* CCAN_LBALANCE_H */
