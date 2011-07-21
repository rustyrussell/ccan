/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_TIME_H
#define CCAN_TIME_H
#include "config.h"
#include <sys/time.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * time_now - return the current time
 *
 * Example:
 *	printf("Now is %lu seconds since epoch\n", (long)time_now().tv_sec);
 */
struct timeval time_now(void);

/**
 * time_greater - is a after b?
 * @a: one time.
 * @b: another time.
 *
 * Example:
 *	static bool timed_out(const struct timeval *start)
 *	{
 *	#define TIMEOUT time_from_msec(1000)
 *		return time_greater(time_now(), time_add(*start, TIMEOUT));
 *	}
 */
bool time_greater(struct timeval a, struct timeval b);

/**
 * time_less - is a before b?
 * @a: one time.
 * @b: another time.
 *
 * Example:
 *	static bool still_valid(const struct timeval *start)
 *	{
 *	#define TIMEOUT time_from_msec(1000)
 *		return time_less(time_now(), time_add(*start, TIMEOUT));
 *	}
 */
bool time_less(struct timeval a, struct timeval b);

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
 *		struct timeval start = time_now();
 *		if (fork() != 0) {
 *			exit(0);
 *		}
 *		wait(NULL);
 *		return time_eq(start, time_now());
 *	}
 */
bool time_eq(struct timeval a, struct timeval b);

/**
 * time_sub - subtract two times
 * @recent: the larger (more recent) time.
 * @old: the smaller (less recent) time.
 *
 * This returns a well formed struct timeval.
 *
 * Example:
 *	static bool was_recent(const struct timeval *start)
 *	{
 *		return time_sub(time_now(), *start).tv_sec < 1;
 *	}
 */
struct timeval time_sub(struct timeval recent, struct timeval old);

/**
 * time_add - add two times
 * @a: one time.
 * @b: another time.
 *
 * The times must not overflow, or the results are undefined.
 *
 * Example:
 *	// We do one every second.
 *	static struct timeval next_time(void)
 *	{
 *		return time_add(time_now(), time_from_msec(1000));
 *	}
 */
struct timeval time_add(struct timeval a, struct timeval b);

/**
 * time_divide - divide a time by a value.
 * @t: a time.
 * @div: number to divide it by.
 *
 * Example:
 *	// How long does it take to do a fork?
 *	static struct timeval forking_time(void)
 *	{
 *		struct timeval start = time_now();
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
struct timeval time_divide(struct timeval t, unsigned long div);

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
struct timeval time_multiply(struct timeval t, unsigned long mult);

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
uint64_t time_to_msec(struct timeval t);

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
uint64_t time_to_usec(struct timeval t);

/**
 * time_from_msec - convert milliseconds to a timeval
 * @msec: time in milliseconds
 *
 * Example:
 *	// 1/2 second timeout
 *	#define TIMEOUT time_from_msec(500)
 */
struct timeval time_from_msec(uint64_t msec);

/**
 * time_from_usec - convert microseconds to a timeval
 * @usec: time in microseconds
 *
 * Example:
 *	// 1/2 second timeout
 *	#define TIMEOUT time_from_usec(500000)
 */
struct timeval time_from_usec(uint64_t usec);

#endif /* CCAN_TIME_H */
