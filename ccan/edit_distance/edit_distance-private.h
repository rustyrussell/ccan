/** @file
 * Private macros and functions for use by the edit_distance implementation.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#ifndef CCAN_EDIT_DISTANCE_PRIVATE_H
#define CCAN_EDIT_DISTANCE_PRIVATE_H

#include "edit_distance.h"

/** Unsafe (arguments evaluated multiple times) 3-value minimum. */
#define ED_MIN2(a, b) ((a) < (b) ? (a) : (b))

/** Unsafe (arguments evaluated multiple times) 3-value minimum. */
#define ED_MIN3(a, b, c) \
	((a) < (b) ? ((a) < (c) ? (a) : (c)) : ((b) < (c) ? (b) : (c)))

/** Swap variables @p a and @p b of a given type. */
#define ED_SWAP(a, b, Type) \
	do { Type swaptmp = a; a = b; b = swaptmp; } while (0)

/** Number of elements in triangular matrix (with diagonal).
 * @param rc Number of rows (or columns, since square). */
#define ED_TMAT_SIZE(rc) (((rc) + 1) * (rc) / 2)

/** Index of element in lower triangular matrix. */
#define ED_TMAT_IND(r, c) (ED_TMAT_SIZE(r) + c)

/**
 * Calculates non-trivial LCS distance (for internal use).
 * @private
 * @param src Source array to calculate distance from (smaller when symmetric).
 * @param slen Number of elements in @p src to consider (must be > 0).
 * @param tgt Target array to calculate distance to.
 * @param tlen Number of elements in @p tgt to consider (must be > 0).
 * @return LCS distance from @p src[0..slen-1] to @p tgt[0..tlen-1].
 */
ed_dist edit_distance_lcs(const ed_elem *src, ed_size slen,
			  const ed_elem *tgt, ed_size tlen);

/**
 * Calculates non-trivial Levenshtein distance (for internal use).
 * @private
 * @param src Source array to calculate distance from (smaller when symmetric).
 * @param slen Number of elements in @p src to consider (must be > 0).
 * @param tgt Target array to calculate distance to.
 * @param tlen Number of elements in @p tgt to consider (must be > 0).
 * @return Levenshtein distance from @p src[0..slen-1] to @p tgt[0..tlen-1].
 */
ed_dist edit_distance_lev(const ed_elem *src, ed_size slen,
			  const ed_elem *tgt, ed_size tlen);

/**
 * Calculates non-trivial Restricted Damerau-Levenshtein distance (for internal
 * use).
 * @private
 * @param src Source array to calculate distance from (smaller when symmetric).
 * @param slen Number of elements in @p src to consider (must be > 0).
 * @param tgt Target array to calculate distance to.
 * @param tlen Number of elements in @p tgt to consider (must be > 0).
 * @return Restricted Damerau-Levenshtein distance from @p src[0..slen-1] to
 * @p tgt[0..tlen-1].
 */
ed_dist edit_distance_rdl(const ed_elem *src, ed_size slen,
			  const ed_elem *tgt, ed_size tlen);

/**
 * Calculates non-trivial Damerau-Levenshtein distance (for internal use).
 * @private
 * @param src Source array to calculate distance from (smaller when symmetric).
 * @param slen Number of elements in @p src to consider (must be > 0).
 * @param tgt Target array to calculate distance to.
 * @param tlen Number of elements in @p tgt to consider (must be > 0).
 * @return Damerau-Levenshtein distance from @p src[0..slen-1] to
 * @p tgt[0..tlen-1].
 */
ed_dist edit_distance_dl(const ed_elem *src, ed_size slen,
			 const ed_elem *tgt, ed_size tlen);

#endif
