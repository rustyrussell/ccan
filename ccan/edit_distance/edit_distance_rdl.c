/** @file
 * Defines Restricted Damerau-Levenshtein distance functions.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#include <stdlib.h>		/* free, malloc */

#include "edit_distance.h"
#include "edit_distance-params.h"
#include "edit_distance-private.h"

ed_dist edit_distance_rdl(const ed_elem *src, ed_size slen,
			  const ed_elem *tgt, ed_size tlen)
{
	/* Optimization: Avoid malloc when required rows of distance matrix can
	 * fit on the stack.
	 */
	ed_dist stackdist[ED_STACK_DIST_VALS];

	/* Two rows of the Wagner-Fischer distance matrix. */
	ed_dist *distmem, *dist, *prevdist;
	if (slen < ED_STACK_DIST_VALS / 2) {
		distmem = stackdist;
		dist = distmem;
		prevdist = distmem + slen + 1;
	} else {
		distmem = malloc((slen + 1) * sizeof(ed_dist) * 2);
		dist = distmem;
		prevdist = distmem + slen + 1;
	}

	/* Initialize row with cost to delete src[0..i-1] */
	dist[0] = 0;
	for (ed_size i = 1; i <= slen; ++i) {
		dist[i] = dist[i - 1] + ED_DEL_COST(src[i - 1]);
	}

	for (ed_size j = 1; j <= tlen; ++j) {
		/* Value for dist[j-2][i-1] (two rows up, one col left). */
		/* Note: dist[0] is not initialized when j == 1, var unused. */
		ed_dist diagdist1 = prevdist[0];
		/* Value for dist[j-2][i-2] (two rows up, two cols left).
		 * Initialization value only used to placate GCC. */
		ed_dist diagdist2 = 0;

		ED_SWAP(dist, prevdist, ed_dist *);

		dist[0] = prevdist[0] + ED_INS_COST(tgt[j - 1]);

		/* Loop invariant: dist[i] is the edit distance between first j
		 * elements of tgt and first i elements of src.
		 */
		for (ed_size i = 1; i <= slen; ++i) {
			ed_dist nextdiagdist = dist[i];

			if (ED_ELEM_EQUAL(src[i - 1], tgt[j - 1])) {
				/* Same as tgt upto j-2, src upto i-2. */
				dist[i] = prevdist[i - 1];
			} else {
				/* Insertion is tgt upto j-2, src upto i-1
				 * + insert tgt[j-1] */
				ed_dist insdist =
				    prevdist[i] + ED_INS_COST(tgt[j - 1]);

				/* Deletion is tgt upto j-1, src upto i-2
				 * + delete src[i-1] */
				ed_dist deldist =
				    dist[i - 1] + ED_DEL_COST(src[i - 1]);

				/* Substitution is tgt upto j-2, src upto i-2
				 * + substitute tgt[j-1] for src[i-1] */
				ed_dist subdist = prevdist[i - 1] +
				    ED_SUB_COST(src[i - 1], tgt[j - 1]);

				/* Use best distance available */
				dist[i] = ED_MIN3(insdist, deldist, subdist);

				if (j > 1 && i > 1 &&
				    ED_ELEM_EQUAL(src[i - 2], tgt[j - 1]) &&
				    ED_ELEM_EQUAL(src[i - 1], tgt[j - 2])) {
					ed_dist tradist = diagdist2 +
					    ED_TRA_COST(src[j - 2], src[j - 1]);
					dist[i] = ED_MIN2(dist[i], tradist);
				}
			}

			diagdist2 = diagdist1;
			diagdist1 = nextdiagdist;
		}
	}

	ed_dist total = dist[slen];
	if (distmem != stackdist) {
		free(distmem);
	}
	return total;
}
