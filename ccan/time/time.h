/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_TIME_H
#define CCAN_TIME_H
#include "config.h"
#include <sys/time.h>
#if HAVE_STRUCT_TIMESPEC
#include <time.h>
#else
struct timespec {
	time_t   tv_sec;        /* seconds */
	long     tv_nsec;       /* nanoseconds */
};
#endif
#include <stdint.h>
#include <stdbool.h>

/**
 * time_now - return the current time
 *
 * Example:
 *	printf("Now is %lu seconds since epoch\n", (long)time_now().tv_sec);
 */
struct timespec time_now(void);

/**
 * time_greater - is a after b?
 * @a: one time.
 * @b: another time.
 *
 * Example:
 *	static bool timed_out(const struct timespec *start)
 *	{
 *	#define TIMEOUT time_from_msec(1000)
 *		return time_greater(time_now(), time_add(*start, TIMEOUT));
 *	}
 */
bool time_greater(struct timespec a, struct timespec b);

/**
 * time_less - is a before b?
 * @a: one time.
 * @b: another time.
 *
 * Example:
 *	static bool still_valid(const struct timespec *start)
 *	{
 *	#define TIMEOUT time_from_msec(1000)
 *		return time_less(time_now(), time_add(*start, TIMEOUT));
 *	}
 */
bool time_less(struct timespec a, struct timespec b);

/**
 * time_eq - is a equal to b?
 * @a: one time.
 * @b: another time.
 *
 * Example:
 *	#include <sys/types.h>
 *	#include <sys/wait.h>
 *
 *	// Can we fork in under a microsecond?
 *	static bool fast_fork(void)
 *	{
 *		struct timespec start = time_now();
 *		if (fork() != 0) {
 *			exit(0);
 *		}
 *		wait(NULL);
 *		return time_eq(start, time_now());
 *	}
 */
bool time_eq(struct timespec a, struct timespec b);

/**
 * time_sub - subtract two times
 * @recent: the larger (more recent) time.
 * @old: the smaller (less recent) time.
 *
 * This returns a well formed struct timespec.
 *
 * Example:
 *	static bool was_recent(const struct timespec *start)
 *	{
 *		return time_sub(time_now(), *start).tv_sec < 1;
 *	}
 */
struct timespec time_sub(struct timespec recent, struct timespec old);

/**
 * time_add - add two times
 * @a: one time.
 * @b: another time.
 *
 * The times must not overflow, or the results are undefined.
 *
 * Example:
 *	// We do one every second.
 *	static struct timespec next_time(void)
 *	{
 *		return time_add(time_now(), time_from_msec(1000));
 *	}
 */
struct timespec time_add(struct timespec a, struct timespec b);

/**
 * time_divide - divide a time by a value.
 * @t: a time.
 * @div: number to divide it by.
 *
 * Example:
 *	// How long does it take to do a fork?
 *	static struct timespec forking_time(void)
 *	{
 *		struct timespec start = time_now();
 *		unsigned int i;
 *
 *		for (i = 0; i < 1000; i++) {
 *			if (fork() != 0) {
 *				exit(0);
 *			}
 *			wait(NULL);
 *		}
 *		return time_divide(time_sub(time_now(), start), i);
 *	}
 */
struct timespec time_divide(struct timespec t, unsigned long div);

/**
 * time_multiply - multiply a time by a value.
 * @t: a time.
 * @mult: number to multiply it by.
 *
 * Example:
 *	...
 *	printf("Time to do 100000 forks would be %u sec\n",
 *	       (unsigned)time_multiply(forking_time(), 1000000).tv_sec);
 */
struct timespec time_multiply(struct timespec t, unsigned long mult);

/**
 * time_to_msec - return number of milliseconds
 * @t: a time
 *
 * It's often more convenient to deal with time values as
 * milliseconds.  Note that this will fit into a 32-bit variable if
 * it's a time difference of less than ~7 weeks.
 *
 * Example:
 *	...
 *	printf("Forking time is %u msec\n",
 *	       (unsigned)time_to_msec(forking_time()));
 */
uint64_t time_to_msec(struct timespec t);

/**
 * time_to_usec - return number of microseconds
 * @t: a time
 *
 * It's often more convenient to deal with time values as
 * microseconds.  Note that this will fit into a 32-bit variable if
 * it's a time difference of less than ~1 hour.
 *
 * Example:
 *	...
 *	printf("Forking time is %u usec\n",
 *	       (unsigned)time_to_usec(forking_time()));
 *
 */
uint64_t time_to_usec(struct timespec t);

/**
 * time_to_nsec - return number of nanoseconds
 * @t: a time
 *
 * It's sometimes more convenient to deal with time values as
 * nanoseconds.  Note that this will fit into a 32-bit variable if
 * it's a time difference of less than ~4 seconds.
 *
 * Example:
 *	...
 *	printf("Forking time is %u nsec\n",
 *	       (unsigned)time_to_nsec(forking_time()));
 *
 */
uint64_t time_to_nsec(struct timespec t);

/**
 * time_from_msec - convert milliseconds to a timespec
 * @msec: time in milliseconds
 *
 * Example:
 *	// 1/2 second timeout
 *	#define TIMEOUT time_from_msec(500)
 */
struct timespec time_from_msec(uint64_t msec);

/**
 * time_from_usec - convert microseconds to a timespec
 * @usec: time in microseconds
 *
 * Example:
 *	// 1/2 second timeout
 *	#define TIMEOUT time_from_usec(500000)
 */
struct timespec time_from_usec(uint64_t usec);

/**
 * time_from_nsec - convert nanoseconds to a timespec
 * @nsec: time in nanoseconds
 *
 * Example:
 *	// 1/2 second timeout
 *	#define TIMEOUT time_from_nsec(500000000)
 */
struct timespec time_from_nsec(uint64_t nsec);

/**
 * timespec_to_timeval - convert a timespec to a timeval.
 * @ts: a timespec.
 *
 * Example:
 *	struct timeval tv;
 *
 *	tv = timespec_to_timeval(time_now());
 */
static inline struct timeval timespec_to_timeval(struct timespec ts)
{
	struct timeval tv;
	tv.tv_sec = ts.tv_sec;
	tv.tv_usec = ts.tv_nsec / 1000;
	return tv;
}

/**
 * timeval_to_timespec - convert a timeval to a timespec.
 * @tv: a timeval.
 *
 * Example:
 *	struct timeval tv = { 0, 500 };
 *	struct timespec ts;
 *
 *	ts = timeval_to_timespec(tv);
 */
static inline struct timespec timeval_to_timespec(struct timeval tv)
{
	struct timespec ts;
	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	return ts;
}
#endif /* CCAN_TIME_H */
