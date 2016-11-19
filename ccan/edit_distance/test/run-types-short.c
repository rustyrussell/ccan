/** @file
 * Runnable tests for the edit_distance module using custom element and
 * distance types.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */

#include <limits.h>		/* USHRT_MAX */

#include <ccan/array_size/array_size.h>
#include <ccan/tap/tap.h>

#define ed_elem unsigned short
#define ed_dist short
#define ED_HASH_ELEM(e) e
#define ED_HASH_MAX USHRT_MAX

#include <ccan/edit_distance/edit_distance.c>
#include <ccan/edit_distance/edit_distance_dl.c>
#include <ccan/edit_distance/edit_distance_lcs.c>
#include <ccan/edit_distance/edit_distance_lev.c>
#include <ccan/edit_distance/edit_distance_rdl.c>

int main(void)
{
	const unsigned short src[] = { 0, 1, 2, 3 };
	const unsigned short tgt[] = { 4, 2, 1, 5 };
	ed_size slen = ARRAY_SIZE(src);
	ed_size tlen = ARRAY_SIZE(tgt);

	plan_tests(4);

	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LCS) == 6);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LEV) == 4);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_RDL) == 3);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_DL) == 3);

	return exit_status();
}
