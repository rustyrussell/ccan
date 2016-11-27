/** @file
 * Defines Damerau-Levenshtein distance functions.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#include <stdlib.h>		/* free, malloc */

#include "edit_distance.h"
#include "edit_distance-params.h"
#include "edit_distance-private.h"

ed_dist edit_distance_dl(const ed_elem *src, ed_size slen,
			 const ed_elem *tgt, ed_size tlen)
{
	/* Optimization: Avoid malloc when distance matrix can fit on the stack.
	 */
	ed_dist stackdist[ED_STACK_DIST_VALS];

	/* Lowrance-Wagner distance matrix, in row-major order. */
	size_t matsize = ((size_t)slen + 2) * (tlen + 2);
	ed_dist *distmem = matsize <= ED_STACK_DIST_VALS ? stackdist :
	    malloc(matsize * sizeof(ed_dist));
	ed_dist *dist = distmem;

#ifdef ED_HASH_ON_STACK
	ed_size lasttgt[ED_HASH_MAX + 1] = { 0 };
#else
	ed_size *lasttgt = calloc(ED_HASH_MAX + 1, sizeof(ed_size));
#endif

	/* Upper bound on distance between strings. */
	ed_dist maxdist = 0;

#ifdef ED_DEL_COST_CONST
	maxdist += (ed_dist)slen *ED_DEL_COST();
#else
	/* Lower-triangular matrix of deletion costs.
	 * delcost[i2, i1] is cost to delete src[i1..i2-1].
	 * delcost[i, i] is 0. */
	ed_dist *delcost = malloc(ED_TMAT_SIZE(slen + 1) * sizeof(ed_dist));
	ed_dist *delcostitr = delcost;
	ed_dist *delcostprevitr = delcost;
	*delcostitr++ = 0;
	for (ed_size i2 = 1; i2 <= slen; ++i2) {
		ed_dist costi2 = ED_DEL_COST(src[i2 - 1]);
		for (ed_size i1 = 0; i1 < i2; ++i1) {
			*delcostitr++ = *delcostprevitr++ + costi2;
		}
		*delcostitr++ = 0;
	}
	maxdist += delcost[ED_TMAT_IND(slen, 0)];
#endif

#ifdef ED_INS_COST_CONST
	maxdist += (ed_dist)tlen *ED_INS_COST();
#else
	/* Lower-triangular matrix of insertion costs.
	 * inscost[j2, j1] is cost to insert tgt[j1..j2-1].
	 * inscost[j, j] is 0. */
	ed_dist *inscost = malloc(ED_TMAT_SIZE(tlen + 1) * sizeof(ed_dist));
	ed_dist *inscostitr = inscost;
	ed_dist *inscostprevitr = inscost;
	*inscostitr++ = 0;
	for (ed_size j2 = 1; j2 <= tlen; ++j2) {
		ed_dist costj2 = ED_INS_COST(tgt[j2 - 1]);
		for (ed_size j1 = 0; j1 < j2; ++j1) {
			*inscostitr++ = *inscostprevitr++ + costj2;
		}
		*inscostitr++ = 0;
	}
	maxdist += inscost[ED_TMAT_IND(tlen, 0)];
#endif

	/* Initialize first row with maximal cost */
	for (ed_size i = 0; i < slen + 2; ++i) {
		dist[i] = maxdist;
	}

	/* Position dist to match other algorithms.  dist[-1] will be maxdist */
	dist += slen + 3;

	/* Initialize row with cost to delete src[0..i-1] */
	dist[-1] = maxdist;
	dist[0] = 0;
	for (ed_size i = 1; i <= slen; ++i) {
		dist[i] = dist[i - 1] + ED_DEL_COST(src[i - 1]);
	}

	for (ed_size j = 1; j <= tlen; ++j) {
		/* Largest y < i such that src[y] = tgt[j] */
		ed_size lastsrc = 0;
		ed_dist *prevdist = dist;
		dist += slen + 2;
		dist[-1] = maxdist;
		dist[0] = prevdist[0] + ED_INS_COST(tgt[j - 1]);

		/* Loop invariant: dist[i] is the edit distance between first j
		 * elements of tgt and first i elements of src.
		 *
		 * Loop invariant: lasttgt[ED_HASH_ELEM(c)] holds the largest
		 * x < j such that tgt[x-1] = c or 0 if no such x exists.
		 */
		for (ed_size i = 1; i <= slen; ++i) {
			ed_size i1 = lastsrc;
			ed_size j1 = lasttgt[ED_HASH_ELEM(src[i - 1])];

			if (ED_ELEM_EQUAL(src[i - 1], tgt[j - 1])) {
				/* Same as tgt upto j-2, src upto i-2. */
				dist[i] = prevdist[i - 1];
				lastsrc = i;
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

				ed_dist swpdist =
				    distmem[(size_t)j1 * (slen + 2) + i1];
#ifdef ED_INS_COST_CONST
				swpdist +=
				    (ed_dist)(j - j1 - 1) * ED_INS_COST();
#else
				swpdist += inscost[ED_TMAT_IND(j - 1, j1)];
#endif
#ifdef ED_TRA_COST_CONST
				swpdist += ED_TRA_COST(,);
#else
				if (i1 > 0) {
					swpdist +=
					    ED_TRA_COST(src[i1 - 1],
							src[i - 1]);
				}
#endif
#ifdef ED_DEL_COST_CONST
				swpdist +=
				    (ed_dist)(i - i1 - 1) * ED_DEL_COST();
#else
				swpdist += delcost[ED_TMAT_IND(i - 1, i1)];
#endif

				dist[i] = ED_MIN2(dist[i], swpdist);
			}
		}

		lasttgt[ED_HASH_ELEM(tgt[j - 1])] = j;
	}

#ifndef ED_HASH_ON_STACK
	free(lasttgt);
#endif

#ifndef ED_DEL_COST_CONST
	free(delcost);
#endif

#ifndef ED_INS_COST_CONST
	free(inscost);
#endif

	ed_dist total = dist[slen];
	if (distmem != stackdist) {
		free(distmem);
	}
	return total;
}
