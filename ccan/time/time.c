/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/time/time.h>
#include <stdlib.h>
#include <assert.h>

struct timeval time_now(void)
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return now;
}

bool time_greater(struct timeval a, struct timeval b)
{
	if (a.tv_sec > b.tv_sec)
		return true;
	else if (a.tv_sec < b.tv_sec)
		 return false;

	return a.tv_usec > b.tv_usec;
}

bool time_less(struct timeval a, struct timeval b)
{
	if (a.tv_sec < b.tv_sec)
		return true;
	else if (a.tv_sec > b.tv_sec)
		 return false;

	return a.tv_usec < b.tv_usec;
}

bool time_eq(struct timeval a, struct timeval b)
{
	return a.tv_sec == b.tv_sec && a.tv_usec == b.tv_usec;
}

struct timeval time_sub(struct timeval recent, struct timeval old)
{
	struct timeval diff;

	diff.tv_sec = recent.tv_sec - old.tv_sec;
	if (old.tv_usec > recent.tv_usec) {
		diff.tv_sec--;
		diff.tv_usec = 1000000 + recent.tv_usec - old.tv_usec;
	} else
		diff.tv_usec = recent.tv_usec - old.tv_usec;

	assert(diff.tv_sec >= 0);
	return diff;
}

struct timeval time_add(struct timeval a, struct timeval b)
{
	struct timeval sum;

	sum.tv_sec = a.tv_sec + b.tv_sec;
	sum.tv_usec = a.tv_usec + b.tv_usec;
	if (sum.tv_usec > 1000000) {
		sum.tv_sec++;
		sum.tv_usec -= 1000000;
	}
	return sum;
}

struct timeval time_divide(struct timeval t, unsigned long div)
{
	return time_from_usec(time_to_usec(t) / div);
}

struct timeval time_multiply(struct timeval t, unsigned long mult)
{
	return time_from_usec(time_to_usec(t) * mult);
}

uint64_t time_to_msec(struct timeval t)
{
	uint64_t msec;

	msec = t.tv_usec / 1000 + (uint64_t)t.tv_sec * 1000;
	return msec;
}

uint64_t time_to_usec(struct timeval t)
{
	uint64_t usec;

	usec = t.tv_usec + (uint64_t)t.tv_sec * 1000000;
	return usec;
}

struct timeval time_from_msec(uint64_t msec)
{
	struct timeval t;

	t.tv_usec = (msec % 1000) * 1000;
	t.tv_sec = msec / 1000;
	return t;
}

struct timeval time_from_usec(uint64_t usec)
{
	struct timeval t;

	t.tv_usec = usec % 1000000;
	t.tv_sec = usec / 1000000;
	return t;
}
