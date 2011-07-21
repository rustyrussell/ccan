/* Licensed under LGPLv3+ - see LICENSE file for details */
#ifndef CCAN_TALLY_H
#define CCAN_TALLY_H
#include "config.h"
#include <sys/types.h>

struct tally;

/**
 * tally_new - allocate the tally structure.
 * @buckets: the number of frequency buckets.
 *
 * This allocates a tally structure using malloc().  The greater the value
 * of @buckets, the more accurate tally_approx_median() and tally_approx_mode()
 * and tally_histogram() will be, but more memory is consumed.  If you want
 * to use tally_histogram(), the optimal bucket value is the same as that
 * @height argument.
 */
struct tally *tally_new(unsigned int buckets);

/**
 * tally_add - add a value.
 * @tally: the tally structure.
 * @val: the value to add.
 */
void tally_add(struct tally *tally, ssize_t val);

/**
 * tally_num - how many times as tally_add been called?
 * @tally: the tally structure.
 */
size_t tally_num(const struct tally *tally);

/**
 * tally_min - the minimum value passed to tally_add.
 * @tally: the tally structure.
 *
 * Undefined if tally_num() == 0.
 */
ssize_t tally_min(const struct tally *tally);

/**
 * tally_max - the maximum value passed to tally_add.
 * @tally: the tally structure.
 *
 * Undefined if tally_num() == 0.
 */
ssize_t tally_max(const struct tally *tally);

/**
 * tally_mean - the mean value passed to tally_add.
 * @tally: the tally structure.
 *
 * Undefined if tally_num() == 0, but will not crash.
 */
ssize_t tally_mean(const struct tally *tally);

/**
 * tally_total - the total value passed to tally_add.
 * @tally: the tally structure.
 * @overflow: the overflow value (or NULL).
 *
 * If your total can't overflow a ssize_t, you don't need @overflow.
 * Otherwise, @overflow is the upper ssize_t, and the return value should
 * be treated as the lower size_t (ie. the sign bit is in @overflow).
 */
ssize_t tally_total(const struct tally *tally, ssize_t *overflow);

/**
 * tally_approx_median - the approximate median value passed to tally_add.
 * @tally: the tally structure.
 * @err: the error in the returned value (ie. real median is +/- @err).
 *
 * Undefined if tally_num() == 0, but will not crash.  Because we
 * don't reallocate, we don't store all values, so this median cannot be
 * exact.
 */
ssize_t tally_approx_median(const struct tally *tally, size_t *err);

/**
 * tally_approx_mode - the approximate mode value passed to tally_add.
 * @tally: the tally structure.
 * @err: the error in the returned value (ie. real mode is +/- @err).
 *
 * Undefined if tally_num() == 0, but will not crash.  Because we
 * don't reallocate, we don't store all values, so this mode cannot be
 * exact.  It could well be a value which was never passed to tally_add!
 */
ssize_t tally_approx_mode(const struct tally *tally, size_t *err);

#define TALLY_MIN_HISTO_WIDTH 8
#define TALLY_MIN_HISTO_HEIGHT 3

/**
 * tally_graph - return an ASCII image of the tally_add distribution
 * @tally: the tally structure.
 * @width: the maximum string width to use (>= TALLY_MIN_HISTO_WIDTH)
 * @height: the maximum string height to use (>= TALLY_MIN_HISTO_HEIGHT)
 *
 * Returns a malloc()ed string which draws a multi-line graph of the
 * distribution of values.  On out of memory returns NULL.
 */
char *tally_histogram(const struct tally *tally,
		      unsigned width, unsigned height);
#endif /* CCAN_TALLY_H */
