/** @file
 * Defines Levenshtein distance functions.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#include <stdlib.h>		/* free, malloc */

#include "edit_distance.h"
#include "edit_distance-params.h"
#include "edit_distance-private.h"

ed_dist edit_distance_lev(const ed_elem *src, ed_size slen,
			  const ed_elem *tgt, ed_size tlen)
{
	/* Optimization: Avoid malloc when row of distance matrix can fit on
	 * the stack.
	 */
	ed_dist stackdist[ED_STACK_DIST_VALS];

	/* One row of the Wagner-Fischer distance matrix. */
	ed_dist *dist = slen < ED_STACK_DIST_VALS ? stackdist :
	    malloc((slen + 1) * sizeof(ed_dist));

	/* Initialize row with cost to delete src[0..i-1] */
	dist[0] = 0;
	for (ed_size i = 1; i <= slen; ++i) {
		dist[i] = dist[i - 1] + ED_DEL_COST(src[i - 1]);
	}

	for (ed_size j = 1; j <= tlen; ++j) {
		/* Value for dist[j-1][i-1] (one row up, one col left). */
		ed_dist diagdist = dist[0];
		dist[0] = dist[0] + ED_INS_COST(tgt[j - 1]);

		/* Loop invariant: dist[i] is the edit distance between first j
		 * elements of tgt and first i elements of src.
		 */
		for (ed_size i = 1; i <= slen; ++i) {
			ed_dist nextdiagdist = dist[i];

			if (ED_ELEM_EQUAL(src[i - 1], tgt[j - 1])) {
				/* Same as tgt upto j-2, src upto i-2. */
				dist[i] = diagdist;
			} else {
				/* Insertion is tgt upto j-2, src upto i-1
				 * + insert tgt[j-1] */
				ed_dist insdist =
				    dist[i] + ED_INS_COST(tgt[j - 1]);

				/* Deletion is tgt upto j-1, src upto i-2
				 * + delete src[i-1] */
				ed_dist deldist =
				    dist[i - 1] + ED_DEL_COST(src[i - 1]);

				/* Substitution is tgt upto j-2, src upto i-2
				 * + substitute tgt[j-1] for src[i-1] */
				ed_dist subdist = diagdist +
				    ED_SUB_COST(src[i - 1], tgt[j - 1]);

				/* Use best distance available */
				dist[i] = ED_MIN3(insdist, deldist, subdist);
			}

			diagdist = nextdiagdist;
		}
	}

	ed_dist total = dist[slen];
	if (dist != stackdist) {
		free(dist);
	}
	return total;
}
