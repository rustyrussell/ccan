/** @file
 * Runnable tests for the edit_distance module using custom element and
 * distance types.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */

#include <limits.h>		/* UCHAR_MAX */

#include <ccan/array_size/array_size.h>
#include <ccan/tap/tap.h>

struct color16 {
	unsigned char r:5;
	unsigned char g:6;
	unsigned char b:5;
};

#define ed_elem struct color16
#define ED_ELEM_EQUAL(e, f) (e.r == f.r && e.g == f.g && e.b == f.b)
#define ED_HASH_ELEM(e) ((e.r << 11) | (e.g << 5) | e.b)
#define ED_HASH_MAX USHRT_MAX

#include <ccan/edit_distance/edit_distance.c>
#include <ccan/edit_distance/edit_distance_dl.c>
#include <ccan/edit_distance/edit_distance_lcs.c>
#include <ccan/edit_distance/edit_distance_lev.c>
#include <ccan/edit_distance/edit_distance_rdl.c>

int main(void)
{
	const struct color16 src[] = {
		{0, 0, 0},
		{1, 1, 1},
		{2, 2, 2},
		{3, 3, 3}
	};
	const struct color16 tgt[] = {
		{4, 4, 4},
		{2, 2, 2},
		{1, 1, 1},
		{5, 5, 5}
	};
	ed_size slen = ARRAY_SIZE(src);
	ed_size tlen = ARRAY_SIZE(tgt);

	plan_tests(4);

	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LCS) == 6);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LEV) == 4);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_RDL) == 3);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_DL) == 3);

	return exit_status();
}
