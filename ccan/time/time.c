/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/time/time.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef DEBUG
#include <ccan/str/str.h>
#define TIME_CHECK(t) time_check((t), __FILE__ ":" stringify(__LINE__))
#else
#define TIME_CHECK(t) (t)
#endif

#if !HAVE_CLOCK_GETTIME && !HAVE_CLOCK_GETTIME_IN_LIBRT
#include <sys/time.h>

struct timespec time_now(void)
{
	struct timeval now;
	struct timespec ret;
	gettimeofday(&now, NULL);
	ret.tv_sec = now.tv_sec;
	ret.tv_nsec = now.tv_usec * 1000;
	return TIME_CHECK(ret);
}
#else
#include <time.h>
struct timespec time_now(void)
{
	struct timespec ret;
	clock_gettime(CLOCK_REALTIME, &ret);
	return TIME_CHECK(ret);
}
#endif /* HAVE_CLOCK_GETTIME || HAVE_CLOCK_GETTIME_IN_LIBRT */

bool time_greater(struct timespec a, struct timespec b)
{
	if (TIME_CHECK(a).tv_sec > TIME_CHECK(b).tv_sec)
		return true;
	else if (a.tv_sec < b.tv_sec)
		 return false;

	return a.tv_nsec > b.tv_nsec;
}

bool time_less(struct timespec a, struct timespec b)
{
	if (TIME_CHECK(a).tv_sec < TIME_CHECK(b).tv_sec)
		return true;
	else if (a.tv_sec > b.tv_sec)
		 return false;

	return a.tv_nsec < b.tv_nsec;
}

bool time_eq(struct timespec a, struct timespec b)
{
	return TIME_CHECK(a).tv_sec == TIME_CHECK(b).tv_sec && a.tv_nsec == b.tv_nsec;
}

struct timespec time_sub(struct timespec recent, struct timespec old)
{
	struct timespec diff;

	diff.tv_sec = TIME_CHECK(recent).tv_sec - TIME_CHECK(old).tv_sec;
	if (old.tv_nsec > recent.tv_nsec) {
		diff.tv_sec--;
		diff.tv_nsec = 1000000000 + recent.tv_nsec - old.tv_nsec;
	} else
		diff.tv_nsec = recent.tv_nsec - old.tv_nsec;

	return TIME_CHECK(diff);
}

struct timespec time_add(struct timespec a, struct timespec b)
{
	struct timespec sum;

	sum.tv_sec = TIME_CHECK(a).tv_sec + TIME_CHECK(b).tv_sec;
	sum.tv_nsec = a.tv_nsec + b.tv_nsec;
	if (sum.tv_nsec >= 1000000000) {
		sum.tv_sec++;
		sum.tv_nsec -= 1000000000;
	}
	return TIME_CHECK(sum);
}

struct timespec time_divide(struct timespec t, unsigned long div)
{
	struct timespec res;
	uint64_t rem, ns;

	/* Dividing seconds is simple. */
	res.tv_sec = TIME_CHECK(t).tv_sec / div;
	rem = t.tv_sec % div;

	/* If we can't fit remainder * 1,000,000,000 in 64 bits? */
#if 0 /* ilog is great, but we use fp for multiply anyway. */
	bits = ilog64(rem);
	if (bits + 30 >= 64) {
		/* Reduce accuracy slightly */
		rem >>= (bits - (64 - 30));
		div >>= (bits - (64 - 30));
	}
#endif
	if (rem & ~(((uint64_t)1 << 30) - 1)) {
		/* FIXME: fp is cheating! */
		double nsec = rem * 1000000000.0 + t.tv_nsec;
		res.tv_nsec = nsec / div;
	} else {
		ns = rem * 1000000000 + t.tv_nsec;
		res.tv_nsec = ns / div;
	}
	return TIME_CHECK(res);
}

struct timespec time_multiply(struct timespec t, unsigned long mult)
{
	struct timespec res;

	/* Are we going to overflow if we multiply nsec? */
	if (mult & ~((1UL << 30) - 1)) {
		/* FIXME: fp is cheating! */
		double nsec = (double)t.tv_nsec * mult;

		res.tv_sec = nsec / 1000000000.0;
		res.tv_nsec = nsec - (res.tv_sec * 1000000000.0);
	} else {
		uint64_t nsec = t.tv_nsec * mult;

		res.tv_nsec = nsec % 1000000000;
		res.tv_sec = nsec / 1000000000;
	}
	res.tv_sec += TIME_CHECK(t).tv_sec * mult;
	return TIME_CHECK(res);
}

uint64_t time_to_msec(struct timespec t)
{
	uint64_t msec;

	msec = TIME_CHECK(t).tv_nsec / 1000000 + (uint64_t)t.tv_sec * 1000;
	return msec;
}

uint64_t time_to_usec(struct timespec t)
{
	uint64_t usec;

	usec = TIME_CHECK(t).tv_nsec / 1000 + (uint64_t)t.tv_sec * 1000000;
	return usec;
}

uint64_t time_to_nsec(struct timespec t)
{
	uint64_t nsec;

	nsec = TIME_CHECK(t).tv_nsec + (uint64_t)t.tv_sec * 1000000000;
	return nsec;
}

struct timespec time_from_msec(uint64_t msec)
{
	struct timespec t;

	t.tv_nsec = (msec % 1000) * 1000000;
	t.tv_sec = msec / 1000;
	return TIME_CHECK(t);
}

struct timespec time_from_usec(uint64_t usec)
{
	struct timespec t;

	t.tv_nsec = (usec % 1000000) * 1000;
	t.tv_sec = usec / 1000000;
	return TIME_CHECK(t);
}

struct timespec time_from_nsec(uint64_t nsec)
{
	struct timespec t;

	t.tv_nsec = nsec % 1000000000;
	t.tv_sec = nsec / 1000000000;
	return TIME_CHECK(t);
}

struct timespec time_check(struct timespec t, const char *abortstr)
{
	if (t.tv_sec < 0 || t.tv_nsec >= 1000000000) {
		if (abortstr) {
			fprintf(stderr, "%s: malformed time %li.%09li\n",
				abortstr,
				(long)t.tv_sec, (long)t.tv_nsec);
			abort();
		} else {
			struct timespec old = t;

			if (t.tv_nsec >= 1000000000) {
				t.tv_sec += t.tv_nsec / 1000000000;
				t.tv_nsec %= 1000000000;
			}
			if (t.tv_sec < 0)
				t.tv_sec = 0;

			fprintf(stderr, "WARNING: malformed time"
				" %li seconds %li ns converted to %li.%09li.\n",
				(long)old.tv_sec, (long)old.tv_nsec,
				(long)t.tv_sec, (long)t.tv_nsec);
		}
	}
	return t;
}
