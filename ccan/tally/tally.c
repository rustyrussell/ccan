/* Licensed under LGPLv3+ - see LICENSE file for details */
#include <ccan/tally/tally.h>
#include <ccan/build_assert/build_assert.h>
#include <ccan/likely/likely.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define SIZET_BITS (sizeof(size_t)*CHAR_BIT)

/* We use power of 2 steps.  I tried being tricky, but it got buggy. */
struct tally {
	ssize_t min, max;
	size_t total[2];
	/* This allows limited frequency analysis. */
	unsigned buckets, step_bits;
	size_t counts[1 /* Actually: [buckets] */ ];
};

struct tally *tally_new(unsigned buckets)
{
	struct tally *tally;

	/* There is always 1 bucket. */
	if (buckets == 0) {
		buckets = 1;
	}

	/* Overly cautious check for overflow. */
	if (sizeof(*tally) * buckets / sizeof(*tally) != buckets) {
		return NULL;
	}

	tally = (struct tally *)malloc(
		sizeof(*tally) + sizeof(tally->counts[0])*(buckets-1));
	if (tally == NULL) {
		return NULL;
	}

	tally->max = ((size_t)1 << (SIZET_BITS - 1));
	tally->min = ~tally->max;
	tally->total[0] = tally->total[1] = 0;
	tally->buckets = buckets;
	tally->step_bits = 0;
	memset(tally->counts, 0, sizeof(tally->counts[0])*buckets);
	return tally;
}

static unsigned bucket_of(ssize_t min, unsigned step_bits, ssize_t val)
{
	/* Don't over-shift. */
	if (step_bits == SIZET_BITS) {
		return 0;
	}
	assert(step_bits < SIZET_BITS);
	return (size_t)(val - min) >> step_bits;
}

/* Return the min value in bucket b. */
static ssize_t bucket_min(ssize_t min, unsigned step_bits, unsigned b)
{
	/* Don't over-shift. */
	if (step_bits == SIZET_BITS) {
		return min;
	}
	assert(step_bits < SIZET_BITS);
	return min + ((ssize_t)b << step_bits);
}

/* Does shifting by this many bits truncate the number? */
static bool shift_overflows(size_t num, unsigned bits)
{
	if (bits == 0) {
		return false;
	}

	return ((num << bits) >> 1) != (num << (bits - 1));
}

/* When min or max change, we may need to shuffle the frequency counts. */
static void renormalize(struct tally *tally,
			ssize_t new_min, ssize_t new_max)
{
	size_t range, spill;
	unsigned int i, old_min;

	/* Uninitialized?  Don't do anything... */
	if (tally->max < tally->min) {
		goto update;
	}

	/* If we don't have sufficient range, increase step bits until
	 * buckets cover entire range of ssize_t anyway. */
	range = (new_max - new_min) + 1;
	while (!shift_overflows(tally->buckets, tally->step_bits)
	       && range > ((size_t)tally->buckets << tally->step_bits)) {
		/* Collapse down. */
		for (i = 1; i < tally->buckets; i++) {
			tally->counts[i/2] += tally->counts[i];
			tally->counts[i] = 0;
		}
		tally->step_bits++;
	}

	/* Now if minimum has dropped, move buckets up. */
	old_min = bucket_of(new_min, tally->step_bits, tally->min);
	memmove(tally->counts + old_min,
		tally->counts,
		sizeof(tally->counts[0]) * (tally->buckets - old_min));
	memset(tally->counts, 0, sizeof(tally->counts[0]) * old_min);

	/* If we moved boundaries, adjust buckets to that ratio. */
	spill = (tally->min - new_min) % (1 << tally->step_bits);
	for (i = 0; i < tally->buckets-1; i++) {
		size_t adjust = (tally->counts[i] >> tally->step_bits) * spill;
		tally->counts[i] -= adjust;
		tally->counts[i+1] += adjust;
	}

update:
	tally->min = new_min;
	tally->max = new_max;
}

void tally_add(struct tally *tally, ssize_t val)
{
	ssize_t new_min = tally->min, new_max = tally->max;
	bool need_renormalize = false;

	if (val < tally->min) {
		new_min = val;
		need_renormalize = true;
	}
	if (val > tally->max) {
		new_max = val;
		need_renormalize = true;
	}
	if (need_renormalize) {
		renormalize(tally, new_min, new_max);
	}

	/* 128-bit arithmetic!  If we didn't want exact mean, we could just
	 * pull it out of counts. */
	if (val > 0 && tally->total[0] + val < tally->total[0]) {
		tally->total[1]++;
	} else if (val < 0 && tally->total[0] + val > tally->total[0]) {
		tally->total[1]--;
	}
	tally->total[0] += val;
	tally->counts[bucket_of(tally->min, tally->step_bits, val)]++;
}

size_t tally_num(const struct tally *tally)
{
	size_t i, num = 0;
	for (i = 0; i < tally->buckets; i++) {
		num += tally->counts[i];
	}
	return num;
}

ssize_t tally_min(const struct tally *tally)
{
	return tally->min;
}

ssize_t tally_max(const struct tally *tally)
{
	return tally->max;
}

/* FIXME: Own ccan module please! */
static unsigned fls64(uint64_t val)
{
#if HAVE_BUILTIN_CLZL
	if (val <= ULONG_MAX) {
		/* This is significantly faster! */
		return val ? sizeof(long) * CHAR_BIT - __builtin_clzl(val) : 0;
	} else {
#endif
	uint64_t r = 64;

	if (!val) {
		return 0;
	}
	if (!(val & 0xffffffff00000000ull)) {
		val <<= 32;
		r -= 32;
	}
	if (!(val & 0xffff000000000000ull)) {
		val <<= 16;
		r -= 16;
	}
	if (!(val & 0xff00000000000000ull)) {
		val <<= 8;
		r -= 8;
	}
	if (!(val & 0xf000000000000000ull)) {
		val <<= 4;
		r -= 4;
	}
	if (!(val & 0xc000000000000000ull)) {
		val <<= 2;
		r -= 2;
	}
	if (!(val & 0x8000000000000000ull)) {
		val <<= 1;
		r -= 1;
	}
	return r;
#if HAVE_BUILTIN_CLZL
	}
#endif
}

/* This is stolen straight from Hacker's Delight. */
static uint64_t divlu64(uint64_t u1, uint64_t u0, uint64_t v)
{
	const uint64_t b = 4294967296ULL; /* Number base (32 bits). */
	uint32_t un[4],		  /* Dividend and divisor */
		vn[2];		  /* normalized and broken */
				  /* up into halfwords. */
	uint32_t q[2];		  /* Quotient as halfwords. */
	uint64_t un1, un0,	  /* Dividend and divisor */
		vn0;		  /* as fullwords. */
	uint64_t qhat;		  /* Estimated quotient digit. */
	uint64_t rhat;		  /* A remainder. */
	uint64_t p;		  /* Product of two digits. */
	int64_t s, i, j, t, k;

	if (u1 >= v) {		  /* If overflow, return the largest */
		return (uint64_t)-1; /* possible quotient. */
	}

	s = 64 - fls64(v);		  /* 0 <= s <= 63. */
	vn0 = v << s;		  /* Normalize divisor. */
	vn[1] = vn0 >> 32;	  /* Break divisor up into */
	vn[0] = vn0 & 0xFFFFFFFF; /* two 32-bit halves. */

	// Shift dividend left.
	un1 = ((u1 << s) | (u0 >> (64 - s))) & (-s >> 63);
	un0 = u0 << s;
	un[3] = un1 >> 32;	  /* Break dividend up into */
	un[2] = un1;		  /* four 32-bit halfwords */
	un[1] = un0 >> 32;	  /* Note: storing into */
	un[0] = un0;		  /* halfwords truncates. */

	for (j = 1; j >= 0; j--) {
		/* Compute estimate qhat of q[j]. */
		qhat = (un[j+2]*b + un[j+1])/vn[1];
		rhat = (un[j+2]*b + un[j+1]) - qhat*vn[1];
	again:
		if (qhat >= b || qhat*vn[0] > b*rhat + un[j]) {
			qhat = qhat - 1;
			rhat = rhat + vn[1];
			if (rhat < b) {
				goto again;
			}
		}

		/* Multiply and subtract. */
		k = 0;
		for (i = 0; i < 2; i++) {
			p = qhat*vn[i];
			t = un[i+j] - k - (p & 0xFFFFFFFF);
			un[i+j] = t;
			k = (p >> 32) - (t >> 32);
		}
		t = un[j+2] - k;
		un[j+2] = t;

		q[j] = qhat;		  /* Store quotient digit. */
		if (t < 0) {		  /* If we subtracted too */
			q[j] = q[j] - 1;  /* much, add back. */
			k = 0;
			for (i = 0; i < 2; i++) {
				t = un[i+j] + vn[i] + k;
				un[i+j] = t;
				k = t >> 32;
			}
			un[j+2] = un[j+2] + k;
		}
	} /* End j. */

	return q[1]*b + q[0];
}

static int64_t divls64(int64_t u1, uint64_t u0, int64_t v)
{
	int64_t q, uneg, vneg, diff, borrow;

	uneg = u1 >> 63;	  /* -1 if u < 0. */
	if (uneg) {		  /* Compute the absolute */
		u0 = -u0;	  /* value of the dividend u. */
		borrow = (u0 != 0);
		u1 = -u1 - borrow;
	}

	vneg = v >> 63;		  /* -1 if v < 0. */
	v = (v ^ vneg) - vneg;	  /* Absolute value of v. */

	if ((uint64_t)u1 >= (uint64_t)v) {
		goto overflow;
	}

	q = divlu64(u1, u0, v);

	diff = uneg ^ vneg;	  /* Negate q if signs of */
	q = (q ^ diff) - diff;	  /* u and v differed. */

	if ((diff ^ q) < 0 && q != 0) {	   /* If overflow, return the
					      largest */
	overflow:			   /* possible neg. quotient. */
		q = 0x8000000000000000ULL;
	}
	return q;
}

ssize_t tally_mean(const struct tally *tally)
{
	size_t count = tally_num(tally);
	if (!count) {
		return 0;
	}

	if (sizeof(tally->total[0]) == sizeof(uint32_t)) {
		/* Use standard 64-bit arithmetic. */
		int64_t total = tally->total[0]
			| (((uint64_t)tally->total[1]) << 32);
		return total / count;
	}
	return divls64(tally->total[1], tally->total[0], count);
}

ssize_t tally_total(const struct tally *tally, ssize_t *overflow)
{
	if (overflow) {
		*overflow = tally->total[1];
		return tally->total[0];
	}

	/* If result is negative, make sure we can represent it. */
	if (tally->total[1] & ((size_t)1 << (SIZET_BITS-1))) {
		/* Must have only underflowed once, and must be able to
		 * represent result at ssize_t. */
		if ((~tally->total[1])+1 != 0
		    || (ssize_t)tally->total[0] >= 0) {
			/* Underflow, return minimum. */
			return (ssize_t)((size_t)1 << (SIZET_BITS - 1));
		}
	} else {
		/* Result is positive, must not have overflowed, and must be
		 * able to represent as ssize_t. */
		if (tally->total[1] || (ssize_t)tally->total[0] < 0) {
			/* Overflow.  Return maximum. */
			return (ssize_t)~((size_t)1 << (SIZET_BITS - 1));
		}
	}
	return tally->total[0];
}

static ssize_t bucket_range(const struct tally *tally, unsigned b, size_t *err)
{
	ssize_t min, max;

	min = bucket_min(tally->min, tally->step_bits, b);
	if (b == tally->buckets - 1) {
		max = tally->max;
	} else {
		max = bucket_min(tally->min, tally->step_bits, b+1) - 1;
	}

	/* FIXME: Think harder about cumulative error; is this enough?. */
	*err = (max - min + 1) / 2;
	/* Avoid overflow. */
	return min + (max - min) / 2;
}

ssize_t tally_approx_median(const struct tally *tally, size_t *err)
{
	size_t count = tally_num(tally), total = 0;
	unsigned int i;

	for (i = 0; i < tally->buckets; i++) {
		total += tally->counts[i];
		if (total * 2 >= count) {
			break;
		}
	}
	return bucket_range(tally, i, err);
}

ssize_t tally_approx_mode(const struct tally *tally, size_t *err)
{
	unsigned int i, min_best = 0, max_best = 0;

	for (i = 0; i < tally->buckets; i++) {
		if (tally->counts[i] > tally->counts[min_best]) {
			min_best = max_best = i;
		} else if (tally->counts[i] == tally->counts[min_best]) {
			max_best = i;
		}
	}

	/* We can have more than one best, making our error huge. */
	if (min_best != max_best) {
		ssize_t min, max;
		min = bucket_range(tally, min_best, err);
		max = bucket_range(tally, max_best, err);
		max += *err;
		*err += (size_t)(max - min);
		return min + (max - min) / 2;
	}

	return bucket_range(tally, min_best, err);
}

static unsigned get_max_bucket(const struct tally *tally)
{
	unsigned int i;

	for (i = tally->buckets; i > 0; i--) {
		if (tally->counts[i-1]) {
			break;
		}
	}
	return i;
}

char *tally_histogram(const struct tally *tally,
		      unsigned width, unsigned height)
{
	unsigned int i, count, max_bucket, largest_bucket;
	struct tally *tmp;
	char *graph, *p;

	assert(width >= TALLY_MIN_HISTO_WIDTH);
	assert(height >= TALLY_MIN_HISTO_HEIGHT);

	/* Ignore unused buckets. */
	max_bucket = get_max_bucket(tally);

	/* FIXME: It'd be nice to smooth here... */
	if (height >= max_bucket) {
		height = max_bucket;
		tmp = NULL;
	} else {
		/* We create a temporary then renormalize so < height. */
		/* FIXME: Antialias properly! */
		tmp = tally_new(tally->buckets);
		if (!tmp) {
			return NULL;
		}
		tmp->min = tally->min;
		tmp->max = tally->max;
		tmp->step_bits = tally->step_bits;
		memcpy(tmp->counts, tally->counts,
		       sizeof(tally->counts[0]) * tmp->buckets);
		while ((max_bucket = get_max_bucket(tmp)) >= height) {
			renormalize(tmp, tmp->min, tmp->max * 2);
		}
		/* Restore max */
		tmp->max = tally->max;
		tally = tmp;
		height = max_bucket;
	}

	/* Figure out longest line, for scale. */
	largest_bucket = 0;
	for (i = 0; i < tally->buckets; i++) {
		if (tally->counts[i] > largest_bucket) {
			largest_bucket = tally->counts[i];
		}
	}

	p = graph = (char *)malloc(height * (width + 1) + 1);
	if (!graph) {
		free(tmp);
		return NULL;
	}

	for (i = 0; i < height; i++) {
		unsigned covered = 1, row;

		/* People expect minimum at the bottom. */
		row = height - i - 1;
		count = (double)tally->counts[row] / largest_bucket * (width-1)+1;

		if (row == 0) {
			covered = snprintf(p, width, "%zi", tally->min);
		} else if (row == height - 1) {
			covered = snprintf(p, width, "%zi", tally->max);
		} else if (row == bucket_of(tally->min, tally->step_bits, 0)) {
			*p = '+';
		} else {
			*p = '|';
		}

		if (covered > width) {
			covered = width;
		}
		p += covered;

		if (count > covered) {
			count -= covered;
		} else {
			count = 0;
		}

		memset(p, '*', count);
		p += count;
		*p = '\n';
		p++;
	}
	*p = '\0';
	free(tmp);
	return graph;
}
