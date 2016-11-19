/** @file
 * Runnable public API tests of the edit_distance module.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */

#include <string.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/edit_distance/edit_distance.h>
#include <ccan/tap/tap.h>

#define edit_distance_lcs_static(arr1, arr2) \
	edit_distance(arr1, sizeof arr1 - 1, arr2, sizeof arr2 - 1, \
			EDIT_DISTANCE_LCS)
#define edit_distance_lev_static(arr1, arr2) \
	edit_distance(arr1, sizeof arr1 - 1, arr2, sizeof arr2 - 1, \
			EDIT_DISTANCE_LEV)
#define edit_distance_rdl_static(arr1, arr2) \
	edit_distance(arr1, sizeof arr1 - 1, arr2, sizeof arr2 - 1, \
			EDIT_DISTANCE_RDL)
#define edit_distance_dl_static(arr1, arr2) \
	edit_distance(arr1, sizeof arr1 - 1, arr2, sizeof arr2 - 1, \
			EDIT_DISTANCE_DL)

static void test_lcs(void)
{
	/* Trivial cases */
	ok1(edit_distance_lcs_static("", "") == 0);
	ok1(edit_distance_lcs_static("a", "") == 1);
	ok1(edit_distance_lcs_static("", "a") == 1);
	ok1(edit_distance_lcs_static("a", "a") == 0);
	ok1(edit_distance_lcs_static("a", "b") == 2);
	ok1(edit_distance_lcs_static("b", "a") == 2);

	/* Trivial search cases */
	ok1(edit_distance_lcs_static("a", "bcdef") == 6);
	ok1(edit_distance_lcs_static("a", "bcadef") == 5);
	ok1(edit_distance_lcs_static("acdef", "b") == 6);
	ok1(edit_distance_lcs_static("abcdef", "b") == 5);

	/* Common prefix with single-char distance */
	ok1(edit_distance_lcs_static("aa", "ab") == 2);
	ok1(edit_distance_lcs_static("ab", "aa") == 2);

	/* Common suffix with single-char distance */
	ok1(edit_distance_lcs_static("aa", "ba") == 2);
	ok1(edit_distance_lcs_static("ba", "aa") == 2);

	/* Non-optimized cases (require Wagner-Fischer matrix) */
	ok1(edit_distance_lcs_static("ab", "ba") == 2);
	ok1(edit_distance_lcs_static("abc", "de") == 5);
	ok1(edit_distance_lcs_static("abc", "def") == 6);

	/* (Restricted) Transposition */
	ok1(edit_distance_lcs_static("abcd", "ecbf") == 6);

	/* (Unrestricted) Transposition */
	ok1(edit_distance_lcs_static("ca", "abc") == 3);

	/* Insert + Delete + Sub */
	ok1(edit_distance_lcs_static("abcde", "xcdef") == 4);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_lcs_static("aabcc", "bccdd") == 4);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_lcs_static("bccdd", "aabcc") == 4);
}

static void test_lev(void)
{
	/* Trivial cases */
	ok1(edit_distance_lev_static("", "") == 0);
	ok1(edit_distance_lev_static("a", "") == 1);
	ok1(edit_distance_lev_static("", "a") == 1);
	ok1(edit_distance_lev_static("a", "a") == 0);
	ok1(edit_distance_lev_static("a", "b") == 1);
	ok1(edit_distance_lev_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_lev_static("a", "bcdef") == 5);
	ok1(edit_distance_lev_static("a", "bcadef") == 5);
	ok1(edit_distance_lev_static("acdef", "b") == 5);
	ok1(edit_distance_lev_static("abcdef", "b") == 5);

	/* Common prefix with single-char distance */
	ok1(edit_distance_lev_static("aa", "ab") == 1);
	ok1(edit_distance_lev_static("ab", "aa") == 1);

	/* Common suffix with single-char distance */
	ok1(edit_distance_lev_static("aa", "ba") == 1);
	ok1(edit_distance_lev_static("ba", "aa") == 1);

	/* Non-optimized cases (require Wagner-Fischer matrix) */
	ok1(edit_distance_lev_static("ab", "ba") == 2);
	ok1(edit_distance_lev_static("abc", "de") == 3);
	ok1(edit_distance_lev_static("abc", "def") == 3);

	/* (Restricted) Transposition */
	ok1(edit_distance_lev_static("abcd", "ecbf") == 4);

	/* (Unrestricted) Transposition */
	ok1(edit_distance_lev_static("ca", "abc") == 3);

	/* Insert + Delete + Sub */
	ok1(edit_distance_lev_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_lev_static("aabcc", "bccdd") == 4);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_lev_static("bccdd", "aabcc") == 4);
}

static void test_rdl(void)
{
	/* Trivial cases */
	ok1(edit_distance_rdl_static("", "") == 0);
	ok1(edit_distance_rdl_static("a", "") == 1);
	ok1(edit_distance_rdl_static("", "a") == 1);
	ok1(edit_distance_rdl_static("a", "a") == 0);
	ok1(edit_distance_rdl_static("a", "b") == 1);
	ok1(edit_distance_rdl_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_rdl_static("a", "bcdef") == 5);
	ok1(edit_distance_rdl_static("a", "bcadef") == 5);
	ok1(edit_distance_rdl_static("acdef", "b") == 5);
	ok1(edit_distance_rdl_static("abcdef", "b") == 5);

	/* Common prefix with single-char distance */
	ok1(edit_distance_rdl_static("aa", "ab") == 1);
	ok1(edit_distance_rdl_static("ab", "aa") == 1);

	/* Common suffix with single-char distance */
	ok1(edit_distance_rdl_static("aa", "ba") == 1);
	ok1(edit_distance_rdl_static("ba", "aa") == 1);

	/* Non-optimized cases (require Wagner-Fischer matrix) */
	ok1(edit_distance_rdl_static("ab", "ba") == 1);
	ok1(edit_distance_rdl_static("abc", "de") == 3);
	ok1(edit_distance_rdl_static("abc", "def") == 3);

	/* (Restricted) Transposition */
	ok1(edit_distance_rdl_static("abcd", "ecbf") == 3);

	/* (Unrestricted) Transposition */
	ok1(edit_distance_rdl_static("ca", "abc") == 3);

	/* Insert + Delete + Sub */
	ok1(edit_distance_rdl_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_rdl_static("aabcc", "bccdd") == 4);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_rdl_static("bccdd", "aabcc") == 4);
}

static void test_dl(void)
{
	/* Trivial cases */
	ok1(edit_distance_dl_static("", "") == 0);
	ok1(edit_distance_dl_static("a", "") == 1);
	ok1(edit_distance_dl_static("", "a") == 1);
	ok1(edit_distance_dl_static("a", "a") == 0);
	ok1(edit_distance_dl_static("a", "b") == 1);
	ok1(edit_distance_dl_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_dl_static("a", "bcdef") == 5);
	ok1(edit_distance_dl_static("a", "bcadef") == 5);
	ok1(edit_distance_dl_static("acdef", "b") == 5);
	ok1(edit_distance_dl_static("abcdef", "b") == 5);

	/* Common prefix with single-char distance */
	ok1(edit_distance_dl_static("aa", "ab") == 1);
	ok1(edit_distance_dl_static("ab", "aa") == 1);

	/* Common suffix with single-char distance */
	ok1(edit_distance_dl_static("aa", "ba") == 1);
	ok1(edit_distance_dl_static("ba", "aa") == 1);

	/* Non-optimized cases (require Wagner-Fischer matrix) */
	ok1(edit_distance_dl_static("ab", "ba") == 1);
	ok1(edit_distance_dl_static("abc", "de") == 3);
	ok1(edit_distance_dl_static("abc", "def") == 3);

	/* (Restricted) Transposition */
	ok1(edit_distance_dl_static("abcd", "ecbf") == 3);

	/* (Unrestricted) Transposition */
	ok1(edit_distance_dl_static("ca", "abc") == 2);

	/* Insert + Delete + Sub */
	ok1(edit_distance_dl_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_dl_static("aabcc", "bccdd") == 4);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_dl_static("bccdd", "aabcc") == 4);
}

int main(void)
{
	plan_tests(89);

	test_lcs();
	test_lev();
	test_rdl();
	test_dl();

	/* Unsupported edit distance measure */
	enum ed_measure badmeasure = (enum ed_measure)-1;
	ok1(edit_distance("ab", 2, "ba", 2, badmeasure) == (ed_dist)-1);

	return exit_status();
}
