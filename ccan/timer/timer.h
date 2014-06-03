/* LGPL (v2.1 or any later version) - see LICENSE file for details */
#ifndef CCAN_TIMER_H
#define CCAN_TIMER_H
#include <ccan/time/time.h>
#include <ccan/list/list.h>
#include <stdint.h>

#ifndef TIMER_GRANULARITY
/* We divide all nsec values by 1000, reducing it to usec granularity. */
#define TIMER_GRANULARITY 1000
#endif

#ifndef TIMER_LEVEL_BITS
/* This gives 32 pointers per level, up to 13 levels deep. */
#define TIMER_LEVEL_BITS 5
#endif

struct timers;
struct timer;

/**
 * timers_init - initialize a timers struct.
 * @timers: the struct timers
 * @start: the minimum time which will ever be added.
 *
 * This sets up a timers struct: any timers added before @start will be
 * set to expire immediately.
 *
 * Example:
 *	struct timers timeouts;
 *
 *	timers_init(&timeouts, time_now());
 */
void timers_init(struct timers *timers, struct timeabs start);

/**
 * timers_cleanup - free allocations within timers struct.
 * @timers: the struct timers
 *
 * This frees any timer layers allocated during use.
 *
 * Example:
 *	timers_cleanup(&timeouts);
 */
void timers_cleanup(struct timers *timers);

/**
 * timer_add - insert a timer.
 * @timers: the struct timers
 * @timer: the (uninitialized) timer to add
 * @when: when @timer expires.
 *
 * This efficiently adds @timer to @timers, to expire @when (rounded to
 * TIMER_GRANULARITY nanoseconds).
 *
 * Example:
 *	struct timer t;
 *
 *	// Timeout in 100ms.
 *	timer_add(&timeouts, &t, timeabs_add(time_now(), time_from_msec(100)));
 */
void timer_add(struct timers *timers, struct timer *timer, struct timeabs when);

/**
 * timer_del - remove an unexpired timer.
 * @timers: the struct timers
 * @timer: the timer previously added with timer_add()
 *
 * This efficiently removes @timer from @timers.
 *
 * Example:
 *	timer_del(&timeouts, &t);
 */
void timer_del(struct timers *timers, struct timer *timer);

/**
 * timer_earliest - find out the first time when a timer will expire
 * @timers: the struct timers
 * @first: the expiry time, only set if there is a timer.
 *
 * This returns false, and doesn't alter @first if there are no
 * timers.  Otherwise, it sets @first to the expiry time of the first
 * timer (rounded to TIMER_GRANULARITY nanoseconds), and returns true.
 *
 * Example:
 *	struct timeabs next = { { (time_t)-1ULL, -1UL } };
 *	timer_earliest(&timeouts, &next);
 */
bool timer_earliest(struct timers *timers, struct timeabs *first);

/**
 * timers_expire - update timers structure and remove expired timers.
 * @timers: the struct timers
 * @expire: the current time
 * @list: the list for expired timers.
 *
 * @list will be initialized to the empty list, then all timers added
 * with a @when arg less than or equal to @expire will be added to it in
 * expiry order (within TIMER_GRANULARITY nanosecond precision).
 *
 * After this, @expire is considered the current time, and adding any
 * timers with @when before this value will be silently changed to
 * adding them with immediate expiration.
 *
 * You should not move @expire backwards, though it need not move
 * forwards.
 *
 * Example:
 *	struct list_head expired;
 *
 *	timers_expire(&timeouts, time_now(), &expired);
 *	if (!list_empty(&expired))
 *		printf("Timer expired!\n");
 */
void timers_expire(struct timers *timers,
		   struct timeabs expire,
		   struct list_head *list);

/**
 * timers_check - check timer structure for consistency
 * @t: the struct timers
 * @abortstr: the location to print on aborting, or NULL.
 *
 * Because timers have redundant information, consistency checking can
 * be done on the tree.  This is useful as a debugging check.  If
 * @abortstr is non-NULL, that will be printed in a diagnostic if the
 * timers structure is inconsistent, and the function will abort.
 *
 * Returns the timers struct if it is consistent, NULL if not (it can
 * never return NULL if @abortstr is set).
 *
 * Example:
 *	timers_check(&timeouts, "After timer_expire");
 */
struct timers *timers_check(const struct timers *t, const char *abortstr);

#ifdef CCAN_TIMER_DEBUG
#include <stdio.h>

/**
 * timers_dump - dump the timers datastructure (for debugging it)
 * @t: the struct timers
 * @fp: the FILE to dump to (stderr if @fp is NULL)
 */
void timers_dump(const struct timers *timers, FILE *fp);
#endif

/**
 * struct timers - structure to hold a set of timers.
 *
 * Initialized using timers_init, the levels of the timer are
 * allocated as necessary, using malloc.
 *
 * See Also:
 *	timers_init(), timers_cleanup()
 */
struct timers {
	/* Far in the future. */
	struct list_head far;
	uint64_t base;
	uint64_t first;

	struct timer_level *level[(64 + TIMER_LEVEL_BITS-1) / TIMER_LEVEL_BITS];
};

/**
 * struct timer - a single timer.
 *
 * Set up by timer_add(), this is usually contained within an
 * application-specific structure.
 *
 * See Also:
 *	ccan/container_of, timer_add(), timer_del()
 */
struct timer {
	struct list_node list;
	uint64_t time;
};
#endif /* CCAN_TIMER_H */
