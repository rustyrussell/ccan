/** @file
 * Runnable tests for the edit_distance module using custom element and
 * distance types.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */

#include <limits.h>		/* UCHAR_MAX */
#include <wchar.h>		/* wmemchr */

#include <ccan/array_size/array_size.h>
#include <ccan/tap/tap.h>

#define ed_elem wchar_t
#define ed_dist double
#define ED_HAS_ELEM(arr, elem, len) (wmemchr(arr, elem, len) != NULL)
/* Since we only use ASCII characters */
#define ED_HASH_ELEM(e) (unsigned char)e
#define ED_HASH_MAX UCHAR_MAX
#define ED_HASH_ON_STACK

#include <ccan/edit_distance/edit_distance.c>
#include <ccan/edit_distance/edit_distance_dl.c>
#include <ccan/edit_distance/edit_distance_lcs.c>
#include <ccan/edit_distance/edit_distance_lev.c>
#include <ccan/edit_distance/edit_distance_rdl.c>

int main(void)
{
	const wchar_t src[] = L"abcd";
	const wchar_t tgt[] = L"ecbf";
	ed_size slen = ARRAY_SIZE(src) - 1;
	ed_size tlen = ARRAY_SIZE(tgt) - 1;

	plan_tests(16);

	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LCS) == 6.0);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_LEV) == 4.0);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_RDL) == 3.0);
	ok1(edit_distance(src, slen, tgt, tlen, EDIT_DISTANCE_DL) == 3.0);

	/* Test empty strings work as expected */
	ok1(edit_distance(src, slen, tgt, 0, EDIT_DISTANCE_LCS) == 4.0);
	ok1(edit_distance(src, 0, tgt, tlen, EDIT_DISTANCE_LCS) == 4.0);

	/* Test ED_HAS_ELEM works as expected */
	ok1(edit_distance(L"c", 1, tgt, tlen, EDIT_DISTANCE_LCS) == 3.0);
	ok1(edit_distance(L"z", 1, tgt, tlen, EDIT_DISTANCE_LCS) == 5.0);
	ok1(edit_distance(src, slen, L"c", 1, EDIT_DISTANCE_LCS) == 3.0);
	ok1(edit_distance(src, slen, L"z", 1, EDIT_DISTANCE_LCS) == 5.0);
	ok1(edit_distance(L"z", 1, tgt, tlen, EDIT_DISTANCE_LEV) == 4.0);
	ok1(edit_distance(src, slen, L"z", 1, EDIT_DISTANCE_LEV) == 4.0);
	ok1(edit_distance(L"z", 1, tgt, tlen, EDIT_DISTANCE_RDL) == 4.0);
	ok1(edit_distance(src, slen, L"z", 1, EDIT_DISTANCE_RDL) == 4.0);
	ok1(edit_distance(L"z", 1, tgt, tlen, EDIT_DISTANCE_DL) == 4.0);
	ok1(edit_distance(src, slen, L"z", 1, EDIT_DISTANCE_DL) == 4.0);

	return exit_status();
}
