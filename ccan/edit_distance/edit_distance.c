/** @file
 * Defines shared edit distance functions.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */
#include "edit_distance-params.h"
#include "edit_distance-private.h"

ed_dist edit_distance(const ed_elem *src, ed_size slen,
		      const ed_elem *tgt, ed_size tlen, enum ed_measure measure)
{
	/* Remove common prefix. */
	while (slen > 0 && tlen > 0 && ED_ELEM_EQUAL(src[0], tgt[0])) {
		++src;
		++tgt;
		--slen;
		--tlen;
	}

	/* Remove common suffix. */
	while (slen > 0 && tlen > 0 &&
	       ED_ELEM_EQUAL(src[slen - 1], tgt[tlen - 1])) {
		--slen;
		--tlen;
	}

#if defined(ED_COST_IS_SYMMETRIC)
	/* Use smaller array for src. */
	if (slen > tlen) {
		ED_SWAP(src, tgt, const ed_elem *);
		ED_SWAP(slen, tlen, ed_size);
	}
#endif

	/* Early return when all insertions. */
	if (slen == 0) {
#ifdef ED_INS_COST_CONST
		return (ed_dist)tlen *ED_INS_COST();
#else
		ed_dist result = 0;
		for (ed_size i = 0; i < tlen; ++i) {
			result += ED_INS_COST(tgt[i]);
		}
		return result;
#endif
	}
#ifndef ED_COST_IS_SYMMETRIC
	/* Early return when all deletions. */
	if (tlen == 0) {
# ifdef ED_DEL_COST_CONST
		return (ed_dist)slen *ED_DEL_COST();
# else
		ed_dist result = 0;
		for (ed_size i = 0; i < slen; ++i) {
			result += ED_DEL_COST(src[i]);
		}
		return result;
# endif
	}
#endif

#if defined(ED_HAS_ELEM) && \
		defined(ED_INS_COST_CONST) && \
		defined(ED_SUB_COST_CONST)
	/* Fast search for single-element source. */
	if (slen == 1) {
		/* Always tlen - 1 inserts (of non-src elements). */
		ed_dist result = (ed_dist)(tlen - 1) * ED_INS_COST();
		if (!ED_HAS_ELEM(tgt, src[0], tlen)) {
			if (measure == EDIT_DISTANCE_LCS) {
				/* If src is not present, delete + insert. */
				result += ED_DEL_COST(src[0]) + ED_INS_COST();
			} else {
				/* If src is not present, substitute it out. */
				result += ED_SUB_COST(,);
			}
		}
		return result;
	}
#endif

#if !defined(ED_COST_IS_SYMMETRIC) && \
		defined(ED_HAS_ELEM) && \
		defined(ED_DEL_COST_CONST) && \
		defined(ED_SUB_COST_CONST)
	/* Fast search for single-element target. */
	if (tlen == 1) {
		/* Always slen - 1 deletes (of non-tgt elements). */
		ed_dist result = (ed_dist)(slen - 1) * ED_DEL_COST();
		if (!ED_HAS_ELEM(src, tgt[0], slen)) {
			if (measure == EDIT_DISTANCE_LCS) {
				/* If tgt is not present, delete + insert. */
				result += ED_INS_COST(tgt[0]) + ED_DEL_COST();
			} else {
				/* If tgt is not present, substitute it out. */
				result += ED_SUB_COST(,);
			}
		}
		return result;
	}
#endif

	switch (measure) {
	case EDIT_DISTANCE_LCS:
		return edit_distance_lcs(src, slen, tgt, tlen);
	case EDIT_DISTANCE_LEV:
		return edit_distance_lev(src, slen, tgt, tlen);
	case EDIT_DISTANCE_RDL:
		return edit_distance_rdl(src, slen, tgt, tlen);
	case EDIT_DISTANCE_DL:
		return edit_distance_dl(src, slen, tgt, tlen);
	}

	return (ed_dist)-1;
}
