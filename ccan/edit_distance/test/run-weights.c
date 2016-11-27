/** @file
 * Runnable tests for the edit_distance module using custom costs/weights.
 *
 * @copyright 2016 Kevin Locke <kevin@kevinlocke.name>
 *            MIT license - see LICENSE file for details
 */

#include <string.h>

#include <ccan/build_assert/build_assert.h>
#include <ccan/tap/tap.h>

#define ED_DEL_COST(e) (e == 'a' ? 2 : 1)
#define ED_INS_COST(e) (e == 'b' ? 2 : 1)
#define ED_SUB_COST(e, f) (f == 'c' && e == 'd' ? 3 : 1)
#define ED_TRA_COST(e, f) (e == 'e' && f == 'f' ? 3 : 1)

#include <ccan/edit_distance/edit_distance.c>
#include <ccan/edit_distance/edit_distance_dl.c>
#include <ccan/edit_distance/edit_distance_lcs.c>
#include <ccan/edit_distance/edit_distance_lev.c>
#include <ccan/edit_distance/edit_distance_rdl.c>

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
	ok1(edit_distance_lcs_static("a", "") == 2);
	ok1(edit_distance_lcs_static("", "a") == 1);
	ok1(edit_distance_lcs_static("a", "a") == 0);
	ok1(edit_distance_lcs_static("a", "b") == 4);
	ok1(edit_distance_lcs_static("b", "a") == 2);

	/* Trivial search cases */
	ok1(edit_distance_lcs_static("a", "bcdef") == 8);
	ok1(edit_distance_lcs_static("a", "bcadef") == 6);
	ok1(edit_distance_lcs_static("acdef", "b") == 8);
	ok1(edit_distance_lcs_static("abcdef", "b") == 6);

	/* Common prefix with single-char distance */
	ok1(edit_distance_lcs_static("aa", "ab") == 4);
	ok1(edit_distance_lcs_static("ab", "aa") == 2);

	/* Common suffix with single-char distance */
	ok1(edit_distance_lcs_static("aa", "ba") == 4);
	ok1(edit_distance_lcs_static("ba", "aa") == 2);

	/* Non-optimized cases (require Wagner-Fischer matrix) */
	ok1(edit_distance_lcs_static("ab", "ba") == 3);
	ok1(edit_distance_lcs_static("abc", "de") == 6);
	ok1(edit_distance_lcs_static("abc", "def") == 7);
	ok1(edit_distance_lcs_static("de", "bdef") == 3);

	/* Transposition + Insert */
	ok1(edit_distance_lcs_static("ca", "abc") == 4);

	/* Insert + Delete + Sub */
	ok1(edit_distance_lcs_static("abcde", "xcdef") == 5);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_lcs_static("aabcc", "bccdd") == 6);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_lcs_static("bccdd", "aabcc") == 4);
}

static void test_lev(void)
{
	/* Trivial cases */
	ok1(edit_distance_lev_static("", "") == 0);
	ok1(edit_distance_lev_static("a", "") == 2);
	ok1(edit_distance_lev_static("", "a") == 1);
	ok1(edit_distance_lev_static("a", "a") == 0);
	ok1(edit_distance_lev_static("a", "b") == 1);
	ok1(edit_distance_lev_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_lev_static("a", "bcdef") == 5);
	ok1(edit_distance_lev_static("a", "bcadef") == 6);
	ok1(edit_distance_lev_static("acdef", "b") == 5);
	ok1(edit_distance_lev_static("abcdef", "b") == 6);

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
	ok1(edit_distance_lev_static("de", "bdef") == 3);

	/* Transposition + Insert */
	ok1(edit_distance_lev_static("ca", "abc") == 3);

	/* Insert + Delete + Sub */
	ok1(edit_distance_lev_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_lev_static("aabcc", "bccdd") == 5);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_lev_static("bccdd", "aabcc") == 4);
}

static void test_rdl(void)
{
	/* Trivial cases */
	ok1(edit_distance_rdl_static("", "") == 0);
	ok1(edit_distance_rdl_static("a", "") == 2);
	ok1(edit_distance_rdl_static("", "a") == 1);
	ok1(edit_distance_rdl_static("a", "a") == 0);
	ok1(edit_distance_rdl_static("a", "b") == 1);
	ok1(edit_distance_rdl_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_rdl_static("a", "bcdef") == 5);
	ok1(edit_distance_rdl_static("a", "bcadef") == 6);
	ok1(edit_distance_rdl_static("acdef", "b") == 5);
	ok1(edit_distance_rdl_static("abcdef", "b") == 6);

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
	ok1(edit_distance_rdl_static("de", "bdef") == 3);

	/* Transposition + Insert */
	ok1(edit_distance_rdl_static("ca", "abc") == 3);

	/* Transpose Weight */
	ok1(edit_distance_rdl_static("ef", "fe") == 2);
	ok1(edit_distance_rdl_static("fe", "ef") == 1);

	/* Insert + Delete + Sub */
	ok1(edit_distance_rdl_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_rdl_static("aabcc", "bccdd") == 5);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_rdl_static("bccdd", "aabcc") == 4);
}

static void test_dl(void)
{
	/* Trivial cases */
	ok1(edit_distance_dl_static("", "") == 0);
	ok1(edit_distance_dl_static("a", "") == 2);
	ok1(edit_distance_dl_static("", "a") == 1);
	ok1(edit_distance_dl_static("a", "a") == 0);
	ok1(edit_distance_dl_static("a", "b") == 1);
	ok1(edit_distance_dl_static("b", "a") == 1);

	/* Trivial search cases */
	ok1(edit_distance_dl_static("a", "bcdef") == 5);
	ok1(edit_distance_dl_static("a", "bcadef") == 6);
	ok1(edit_distance_dl_static("acdef", "b") == 5);
	ok1(edit_distance_dl_static("abcdef", "b") == 6);

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
	ok1(edit_distance_dl_static("de", "bdef") == 3);

	/* Transposition + Insert */
	ok1(edit_distance_dl_static("ca", "abc") == 3);

	/* Transpose Weight */
	ok1(edit_distance_dl_static("ef", "fe") == 2);
	ok1(edit_distance_dl_static("fe", "ef") == 1);

	/* Insert + Delete + Sub */
	ok1(edit_distance_dl_static("abcde", "xcdef") == 3);

	/* Distance depends on multiple deletions in final row. */
	ok1(edit_distance_dl_static("aabcc", "bccdd") == 5);

	/* Distance depends on multiple insertions in final column. */
	ok1(edit_distance_dl_static("bccdd", "aabcc") == 4);
}

/* Test edit_distance calculation around the stack threshold to ensure memory
 * is allocated and freed correctly and stack overflow does not occur.
 *
 * Note:  This test is done when ED_COST_IS_SYMMETRIC is not defined so that
 * tgt can be small to make the test run quickly (with ED_COST_IS_SYMMETRIC the
 * min length would need to be above the threshold).
 */
static void test_mem_use(void)
{
	char tgt[] = "BC";
	char src[ED_STACK_DIST_VALS + 1];
	for (size_t i = 0; i < ED_STACK_DIST_VALS; ++i) {
		src[i] = (char)('A' + (i % 26));
	}
	src[ED_STACK_DIST_VALS] = '\0';

	for (ed_size tlen = 1; tlen < 3; ++tlen) {
		ed_size slen = ED_STACK_DIST_VALS;
		/* Above threshold, causes allocation */
		ok(edit_distance_lcs(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_lcs(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_lev(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_lev(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_rdl(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_rdl(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_dl(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_dl(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);

		/* Below threshold, no allocation */
		--slen;
		ok(edit_distance_lcs(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_lcs(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_lev(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_lev(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_rdl(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_rdl(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
		ok(edit_distance_dl(src, slen, tgt, tlen) == slen - tlen,
		   "edit_distance_dl(\"%.3s..., %u, \"%.*s\", %u) == %u",
		   src, slen, (int)tlen, tgt, tlen, slen - tlen);
	}
}

int main(void)
{
	plan_tests(109);

	test_lcs();
	test_lev();
	test_rdl();
	test_dl();

	test_mem_use();

	/* Unsupported edit distance measure */
	enum ed_measure badmeasure = (enum ed_measure)-1;
	ok1(edit_distance("ab", 2, "ba", 2, badmeasure) == (ed_dist)-1);

	return exit_status();
}
