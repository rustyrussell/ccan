/* Licensed under GPLv3+ - see LICENSE file for details */
#ifndef CCAN_LBALANCE_H
#define CCAN_LBALANCE_H
#include "config.h"

struct lbalance;
struct lbalance_task;
struct timeval;
struct rusage;

/**
 * lbalance_new - initialize a load balancing structure.
 *
 * Example:
 *	struct lbalance *lb = lbalance_new();
 *
 *	// ...
 *
 *	lbalance_free(lb);
 *	return 0;
 */
struct lbalance *lbalance_new(void);

/**
 * lbalance_free - free a load balancing structure.
 * @lbalance: the load balancer from lbalance_new.
 *
 * Also frees any tasks still attached.
 */
void lbalance_free(struct lbalance *lbalance);

/**
 * lbalance_task_new - mark the starting of a new task.
 * @lbalance: the load balancer from lbalance_new.
 *
 * Example:
 *	static pid_t run_child(struct lbalance *lb, struct lbalance_task **task)
 *	{
 *		pid_t pid = fork();
 *		if (pid != 0) {
 *			// We are the parent, return.
 *			*task = lbalance_task_new(lb);
 *			return pid;
 *		}
 *		// otherwise do some work...
 *		exit(0);
 *	}
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
 *
 * Example:
 *	#include <sys/types.h>
 *	#include <sys/time.h>
 *	#include <sys/resource.h>
 *	#include <sys/wait.h>
 *
 *	static void wait_for_child(struct lbalance_task *task)
 *	{
 *		struct rusage ru;
 *		// Wait for child to finish, get usage.
 *		wait4(-1, NULL, 0, &ru);
 *		// Tell lbalancer about usage, free struct lbalance_task.
 *		lbalance_task_free(task, &ru);
 *	}
 */
void lbalance_task_free(struct lbalance_task *task,
			const struct rusage *usage);

/**
 * lbalance_target - how many tasks in parallel are recommended?
 * @lbalance: the load balancer from lbalance_new.
 *
 * Normally you keep creating tasks until this limit is reached.  It's
 * updated by stats from lbalance_task_free.
 *
 * Example:
 *	int main(void)
 *	{
 *		unsigned int num_running = 0;
 *		struct lbalance *lb = lbalance_new();
 *
 *		for (;;) {
 *			// Run more until we reach target.
 *			while (num_running < lbalance_target(lb)) {
 *				// Run another one.
 *			}
 *
 *			// Wait for something to finish.
 *		}
 *	}
 */
unsigned lbalance_target(struct lbalance *lbalance);

#endif /* CCAN_LBALANCE_H */
