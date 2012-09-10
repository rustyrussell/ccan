/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/time/time.h>
#include <stdlib.h>
#include <stdio.h>

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
