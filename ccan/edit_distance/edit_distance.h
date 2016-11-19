/** @file
 * Main header file for edit_distance which defines the module API.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#ifndef CCAN_EDIT_DISTANCE_H
#define CCAN_EDIT_DISTANCE_H

#include <stddef.h>		/* size_t */

#ifndef ed_dist
/**
 * ed_dist - Type in which the edit distance is expressed.
 *
 * The name @c ed_dist can be defined to another algebraic type (or this
 * typedef can be edited) when compiling this module.
 */
typedef unsigned int ed_dist;
#endif

#ifndef ed_elem
/**
 * ed_elem - Element type of arrays for which the edit distance is calculated.
 *
 * Changing this type often requires changing #ED_HAS_ELEM, #ED_HASH_ELEM, and
 * #ED_HASH_MAX.  It requires changing #ED_ELEM_EQUAL for non-primitive types.
 * These can be changed in edit_distance-params.h or defined externally.
 */
typedef char ed_elem;
#endif

#ifndef ed_size
/**
 * ed_size - Type in which the array size is expressed.
 *
 * The name @c ed_size can be defined to another algebraic type (or this
 * typedef can be edited) when compiling this module.  If @c ed_size is a
 * signed type, the caller must ensure passed values are non-negative.
 * @c ed_size must be large enough to hold @c max(slen,tlen)+2, it does not
 * need to be large enough to hold @c slen*tlen.
 *
 * Note: @c ed_size is only likely to have a noticeable impact in
 * edit_distance_dl() which maintains an array of @c #ED_HASH_MAX+1 ::ed_size
 * values.
 */
typedef unsigned int ed_size;
#endif

/**
 * ed_measure - Supported edit distance measures.
 */
enum ed_measure {
	/**
	 * Longest Common Subsequence (LCS) distance.
	 *
	 * The LCS distance is an edit distance measured by the number of
	 * insert and delete operations necessary to turn @p src into @p tgt.
	 *
	 * This implementation uses an iterative version of the Wagner-Fischer
	 * algorithm @cite Wagner74 which requires <code>O(slen * tlen)</code>
	 * time and <code>min(slen, tlen) + 1</code> space.
	 */
	EDIT_DISTANCE_LCS = 1,
	/**
	 * Levenshtein distance.
	 *
	 * The Levenshtein distance is an edit distance measured by the number
	 * of insert, delete, and substitute operations necessary to turn
	 * @p src into @p tgt as described by Vladimir Levenshtein.
	 * @cite Levenshtein66
	 *
	 * This implementation uses a modified version of the Wagner-Fischer
	 * algorithm @cite Wagner74 which requires <code>O(slen * tlen)</code>
	 * time and only <code>min(slen, tlen) + 1</code> space.
	 */
	EDIT_DISTANCE_LEV,
	/**
	 * Restricted Damerau-Levenshtein distance (aka Optimal String
	 * Alignment distance).
	 *
	 * The Restricted Damerau-Levenshtein distance is an edit distance
	 * measured by the number of insert, delete, substitute, and transpose
	 * operations necessary to turn @p src into @p tgt with the restriction
	 * that no substring is edited more than once (equivalently, that no
	 * edits overlap). @cite Boytsov11
	 *
	 * This implementation uses a modified version of the Wagner-Fischer
	 * algorithm @cite Wagner74 which requires <code>O(slen * tlen)</code>
	 * time and only <code>2 * min(slen, tlen) + 2</code> space.
	 */
	EDIT_DISTANCE_RDL,
	/**
	 * Damerau-Levenshtein distance.
	 *
	 * The Damerau-Levenshtein distance is an edit distance measured by the
	 * number of insert, delete, substitute, and transpose operations
	 * necessary to turn @p src into @p tgt . @cite Damerau64
	 *
	 * This implementation uses the Lowrance-Wagner algorithm @cite Wagner75
	 * which takes <code>O(slen * tlen)</code> time and
	 * <code>O(slen * tlen)</code> space.
	 */
	EDIT_DISTANCE_DL,
};

/**
 * edit_distance - Calculates the edit distance between two arrays.
 *
 * @param src Source array to calculate distance from.
 * @param slen Number of elements in @p src to consider.
 * @param tgt Target array to calculate distance to.
 * @param tlen Number of elements in @p tgt to consider.
 * @param measure Edit distance measure to calculate.
 * @return Edit distance from @p src[0..slen-1] to @p tgt[0..tlen-1].  (i.e.
 * The minimal sum of the weights of the operations necessary to convert
 * @p src[0..slen-1] into @p tgt[0..tlen-1].)
 *
 * @code
 * Example:
 * const char *source = "kitten";
 * const char *target = "sitting";
 * assert(edit_distance(source, strlen(source),
 *                      target, strlen(target),
 *                      EDIT_DISTANCE_DL) == 3);
 * Example_End: @endcode
 */
ed_dist edit_distance(const ed_elem *src, ed_size slen,
		      const ed_elem *tgt, ed_size tlen,
		      enum ed_measure measure);
#endif
