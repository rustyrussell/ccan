/* Licensed under GPLv3+ - see LICENSE file for details */
#include <ccan/lbalance/lbalance.h>
#include <ccan/tlist2/tlist2.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>

struct stats {
	/* How many stats of for this value do we have? */
	unsigned int num_stats;
	/* What was our total work rate? */
	float work_rate;
};

struct lbalance;

struct lbalance_task {
	struct lbalance *lb;
	struct list_node list;

	/* The time this task started */
	struct timeval start;
	float tasks_sum_start;
};

struct lbalance {
	TLIST2(struct lbalance_task, list) tasks;
	unsigned int num_tasks;

	/* We figured out how many we want to run. */
	unsigned int target;
	/* We need to recalc once a report comes in via lbalance_task_free. */
	bool target_uptodate;

	/* Integral of how many tasks were running so far */
	struct timeval prev_tasks_time;
	float tasks_sum;

	/* For differential rusage. */
	struct rusage prev_usage;

	/* How many stats we have collected (we invalidate old ones). */
	unsigned int total_stats;

	/* Array of stats, indexed by number of tasks we were running. */
	unsigned int max_stats;
	struct stats *stats;
};

struct lbalance *lbalance_new(void)
{
	struct lbalance *lb = malloc(sizeof *lb);
	if (!lb)
		return NULL;

	tlist2_init(&lb->tasks);
	lb->num_tasks = 0;
	gettimeofday(&lb->prev_tasks_time, NULL);
	lb->tasks_sum = 0.0;

	getrusage(RUSAGE_CHILDREN, &lb->prev_usage);

	lb->max_stats = 1;
	lb->stats = malloc(sizeof(lb->stats[0]) * lb->max_stats);
	if (!lb->stats) {
		free(lb);
		return NULL;
	}
	lb->stats[0].num_stats = 0;
	lb->stats[0].work_rate = 0.0;
	lb->total_stats = 0;

	/* Start with # CPUS as a guess. */
	lb->target = -1L;
#ifdef _SC_NPROCESSORS_ONLN
	lb->target = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROCESSORS_CONF)
	if (lb->target == (unsigned int)-1L)
		lb->target = sysconf(_SC_NPROCESSORS_CONF);
#endif
	/* Otherwise, two is a good number. */
	if (lb->target == (unsigned int)-1L || lb->target < 2)
		lb->target = 2;
	lb->target_uptodate = true;

	return lb;
}

/* Return time differences in usec */
static float timeval_sub(struct timeval recent, struct timeval old)
{
	float diff;

	if (old.tv_usec > recent.tv_usec) {
		diff = 1000000 + recent.tv_usec - old.tv_usec;
		recent.tv_sec--;
	} else
		diff = recent.tv_usec - old.tv_usec;

	diff += (float)(recent.tv_sec - old.tv_sec) * 1000000;
	return diff;
}

/* There were num_tasks running between prev_tasks_time and now. */
static void update_tasks_sum(struct lbalance *lb,
			     const struct timeval *now)
{
	lb->tasks_sum += timeval_sub(*now, lb->prev_tasks_time)
		* lb->num_tasks;
	lb->prev_tasks_time = *now;
}

struct lbalance_task *lbalance_task_new(struct lbalance *lb)
{
	struct lbalance_task *task = malloc(sizeof *task);
	if (!task)
		return NULL;

	if (lb->num_tasks + 1 == lb->max_stats) {
		struct stats *s = realloc(lb->stats,
					  sizeof(*s) * (lb->max_stats + 1));
		if (!s) {
			free(task);
			return NULL;
		}
		lb->stats = s;
		lb->stats[lb->max_stats].num_stats = 0;
		lb->stats[lb->max_stats].work_rate = 0.0;
		lb->max_stats++;
	}

	task->lb = lb;
	gettimeofday(&task->start, NULL);

	/* Record that we ran num_tasks up until now. */
	update_tasks_sum(lb, &task->start);

	task->tasks_sum_start = lb->tasks_sum;
	tlist2_add_tail(&lb->tasks, task);
	lb->num_tasks++;

	return task;
}

/* We slowly erase old stats, once we have enough. */
static void degrade_stats(struct lbalance *lb)
{
	unsigned int i;

	if (lb->total_stats < lb->max_stats * 16)
		return;

#if 0
	fprintf(stderr, ".");
#endif
	for (i = 0; i < lb->max_stats; i++) {
		struct stats *s = &lb->stats[i];
		unsigned int stats_lost = (s->num_stats + 1) / 2;
		s->work_rate *= (float)(s->num_stats - stats_lost)
			/ s->num_stats;
		s->num_stats -= stats_lost;
		lb->total_stats -= stats_lost;
		if (s->num_stats == 0)
			s->work_rate = 0.0;
	}
}

static void add_to_stats(struct lbalance *lb,
			 unsigned int num_tasks,
			 float work_rate)
{
#if 0
	fprintf(stderr, "With %.2f running, work rate was %.5f\n",
		num_tasks, work_rate);
#endif
	assert(num_tasks >= 1);
	assert(num_tasks < lb->max_stats);

	lb->stats[num_tasks].num_stats++;
	lb->stats[num_tasks].work_rate += work_rate;
	lb->total_stats++;
	lb->target_uptodate = false;
}

void lbalance_task_free(struct lbalance_task *task,
			const struct rusage *usage)
{
	float work_done, duration;
	unsigned int num_tasks;
	struct timeval now;
	struct rusage ru;

	gettimeofday(&now, NULL);
	duration = timeval_sub(now, task->start);
	
	getrusage(RUSAGE_CHILDREN, &ru);
	if (usage) {
		work_done = usage->ru_utime.tv_usec + usage->ru_stime.tv_usec
			+ (usage->ru_utime.tv_sec + usage->ru_stime.tv_sec)
			* 1000000;
	} else {
		/* Take difference in rusage as rusage of that task. */
		work_done = timeval_sub(ru.ru_utime,
					task->lb->prev_usage.ru_utime)
			+ timeval_sub(ru.ru_stime,
				      task->lb->prev_usage.ru_utime);
	}
	/* Update previous usage. */
	task->lb->prev_usage = ru;

	/* Record that we ran num_tasks up until now. */
	update_tasks_sum(task->lb, &now);

	/* So, on average, how many tasks were running during this time? */
	num_tasks = (task->lb->tasks_sum - task->tasks_sum_start)
		/ duration + 0.5;

	/* Record the work rate for that many tasks. */
	add_to_stats(task->lb, num_tasks, work_done / duration);

	/* We throw away old stats. */
	degrade_stats(task->lb);

	/* We need to recalculate the target. */
	task->lb->target_uptodate = false;

	/* Remove this task. */
	tlist2_del_from(&task->lb->tasks, task);
	task->lb->num_tasks--;
	free(task);
}

/* We look for the point where the work rate starts to drop.  Say you have
 * 4 cpus, we'd expect the work rate for 5 processes to drop 20%.
 *
 * If we're within 1/4 of that ideal ratio, we assume it's still
 * optimal.  Any drop of more than 1/2 is interpreted as the point we
 * are overloaded. */
static unsigned int best_target(const struct lbalance *lb)
{
	unsigned int i, found_drop = 0;
	float best_f_max = -1.0, cliff = -1.0;

#if 0
	for (i = 1; i < lb->max_stats; i++) {
		printf("%u: %f (%u)\n", i,
		       lb->stats[i].work_rate / lb->stats[i].num_stats,
		       lb->stats[i].num_stats);
	}
#endif

	for (i = 1; i < lb->max_stats; i++) {
		float f;

		if (!lb->stats[i].num_stats)
			f = 0;
		else
			f = lb->stats[i].work_rate / lb->stats[i].num_stats;

		if (f > best_f_max) {
#if 0
			printf("Best is %i\n", i);
#endif
			best_f_max = f - (f / (i + 1)) / 4;
			cliff = f - (f / (i + 1)) / 2;
			found_drop = 0;
		} else if (!found_drop && f < cliff) {
#if 0
			printf("Found drop at %i\n", i);
#endif
			found_drop = i;
		}
	}

	if (found_drop) {
		return found_drop - 1;
	}
	return i - 1;
}

static unsigned int calculate_target(struct lbalance *lb)
{
	unsigned int target;

	target = best_target(lb);

	/* Jitter if the adjacent ones are unknown. */
	if (target >= lb->max_stats || lb->stats[target].num_stats == 0)
		return target;

	if (target + 1 == lb->max_stats || lb->stats[target+1].num_stats == 0)
		return target + 1;

	if (target > 1 && lb->stats[target-1].num_stats == 0)
		return target - 1;

	return target;
}

unsigned lbalance_target(struct lbalance *lb)
{
	if (!lb->target_uptodate) {
		lb->target = calculate_target(lb);
		lb->target_uptodate = true;
	}
	return lb->target;
}
	
void lbalance_free(struct lbalance *lb)
{
	struct lbalance_task *task;

	while ((task = tlist2_top(&lb->tasks))) {
		assert(task->lb == lb);
		tlist2_del_from(&lb->tasks, task);
		lb->num_tasks--;
		free(task);
	}
	assert(lb->num_tasks == 0);
	free(lb->stats);
	free(lb);
}
