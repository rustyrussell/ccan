#include "config.h"

#include <ccan/bytestring/bytestring.h>
#include <ccan/tap/tap.h>

#include <ccan/bytestring/bytestring.c>

#define TEST_STRING	"test string"
#define TEST_STRING_2	"abc\0def"

const char str1[] = TEST_STRING;
const char *str2 = TEST_STRING;

int main(void)
{
	struct bytestring bs, bs1, bs2, bs3, bs4, bs5, bs6, bs7;
	int n;

	/* This is how many tests you plan to run */
	plan_tests(126);

	bs = bytestring(str1, sizeof(str1) - 1);
	ok1(bs.ptr == str1);
	ok1(bs.len == (sizeof(str1) - 1));

	bs1 = BYTESTRING(TEST_STRING);
	ok1(bytestring_eq(bs, bs1));

	bs2 = BYTESTRING(TEST_STRING_2);
	ok1(bs2.len == 7);

	bs3 = bytestring_from_string(str2);
	ok1(bytestring_eq(bs3, bs));

	bs4 = bytestring_from_string(TEST_STRING_2);
	ok1(bs4.len == 3);

	bs5 = bytestring_from_string(NULL);
	ok1(bs5.len == 0);
	ok1(bs5.ptr == NULL);
	ok1(bytestring_eq(bs5, bytestring_NULL));

	ok1(bytestring_byte(bs2, 0) == 'a');
	ok1(bytestring_byte(bs2, 1) == 'b');
	ok1(bytestring_byte(bs2, 2) == 'c');
	ok1(bytestring_byte(bs2, 3) == '\0');
	ok1(bytestring_byte(bs2, 4) == 'd');
	ok1(bytestring_byte(bs2, 5) == 'e');
	ok1(bytestring_byte(bs2, 6) == 'f');

	ok1(bytestring_eq(bytestring_slice(bs, 0, 4), BYTESTRING("test")));
	ok1(bytestring_eq(bytestring_slice(bs, 5, 8), BYTESTRING("str")));
	ok1(bytestring_eq(bytestring_slice(bs2, 2, 5), BYTESTRING("c\0d")));
	ok1(bytestring_eq(bytestring_slice(bs2, 0, -1U), bs2));
	ok1(bytestring_eq(bytestring_slice(bs2, 10, 20), bytestring_NULL));
	ok1(bytestring_eq(bytestring_slice(bs2, 2, 1), bytestring_NULL));

	ok1(bytestring_starts(bs, BYTESTRING("test")));
	ok1(bytestring_ends(bs, BYTESTRING("string")));
	ok1(bytestring_starts(bs2, BYTESTRING("abc")));
	ok1(bytestring_starts(bs2, BYTESTRING("abc\0")));
	ok1(bytestring_ends(bs2, BYTESTRING("def")));
	ok1(bytestring_ends(bs2, BYTESTRING("\0def")));
	ok1(!bytestring_starts(bs2, BYTESTRING("def")));
	ok1(!bytestring_ends(bs2, BYTESTRING("abc")));

	ok1(bytestring_index(bs1, ' ') == (bs1.ptr + 4));
	ok1(bytestring_index(bs1, 't') == (bs1.ptr + 0));
	ok1(bytestring_index(bs1, 0) == NULL);
	ok1(bytestring_index(bs2, 0) == (bs2.ptr + 3));
	ok1(bytestring_index(bs2, 'f') == (bs2.ptr + 6));
	ok1(bytestring_index(bs2, 'q') == NULL);

	ok1(bytestring_rindex(bs1, ' ') == (bs1.ptr + 4));
	ok1(bytestring_rindex(bs1, 't') == (bs1.ptr + 6));
	ok1(bytestring_rindex(bs1, 0) == NULL);
	ok1(bytestring_rindex(bs2, 0) == (bs2.ptr + 3));
	ok1(bytestring_rindex(bs2, 'f') == (bs2.ptr + 6));
	ok1(bytestring_rindex(bs2, 'q') == NULL);

	bs6 = BYTESTRING("string");
	ok1(bytestring_eq(bytestring_bytestring(bs1, bs6),
			  bytestring(bs1.ptr + 5, 6)));
	bs6 = BYTESTRING("c\0d");
	ok1(bytestring_eq(bytestring_bytestring(bs2, bs6),
			  bytestring(bs2.ptr + 2, 3)));
	bs6 = BYTESTRING("c\0e");
	ok1(bytestring_bytestring(bs2, bs6).ptr == NULL);
	ok1(bytestring_eq(bytestring_bytestring(bs1, bytestring_NULL),
			  bytestring(bs1.ptr, 0)));
	ok1(bytestring_eq(bytestring_bytestring(bs2, bytestring_NULL),
			  bytestring(bs2.ptr, 0)));


	ok1(bytestring_spn(bs1, BYTESTRING("est")) == 4);
	ok1(bytestring_cspn(bs1, BYTESTRING(" ")) == 4);

	ok1(bytestring_spn(bs2, BYTESTRING("z")) == 0);
	ok1(bytestring_cspn(bs2, BYTESTRING("\0")) == 3);

	ok1(bytestring_spn(bs1, BYTESTRING("eginrst ")) == bs1.len);
	ok1(bytestring_cspn(bs2, BYTESTRING("z")) == bs2.len);

	bs7 = bytestring_splitchr_first(bs, ' ');
	ok1(bs7.ptr == bs.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("test")));
	bs7 = bytestring_splitchr_next(bs, ' ', bs7);
	ok1(bs7.ptr == bs.ptr + 5);
	ok1(bytestring_eq(bs7, BYTESTRING("string")));
	bs7 = bytestring_splitchr_next(bs, ' ', bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchr_next(bs, ' ', bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchr_first(bs2, '\0');
	ok1(bs7.ptr = bs2.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("abc")));
	bs7 = bytestring_splitchr_next(bs2, '\0', bs7);
	ok1(bs7.ptr == bs2.ptr + 4);
	ok1(bytestring_eq(bs7, BYTESTRING("def")));
	bs7 = bytestring_splitchr_next(bs2, '\0', bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchr_next(bs2, ' ', bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchr_first(bs, 's');
	ok1(bs7.ptr == bs.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("te")));
	bs7 = bytestring_splitchr_next(bs, 's', bs7);
	ok1(bs7.ptr == bs.ptr + 3);
	ok1(bytestring_eq(bs7, BYTESTRING("t ")));
	bs7 = bytestring_splitchr_next(bs, 's', bs7);
	ok1(bs7.ptr == bs.ptr + 6);
	ok1(bytestring_eq(bs7, BYTESTRING("tring")));
	bs7 = bytestring_splitchr_next(bs, 's', bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchr_next(bs, ' ', bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchr_first(bs2, 'f');
	ok1(bs7.ptr = bs2.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("abc\0de")));
	bs7 = bytestring_splitchr_next(bs2, 'f', bs7);
	ok1(bs7.ptr == bs2.ptr + 7);
	ok1(bs7.len == 0);
	bs7 = bytestring_splitchr_next(bs2, 'f', bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchr_next(bs2, 'f', bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchr_first(BYTESTRING(""), 'q');
	ok1(!bs7.ptr);

	n = 0;
	bytestring_foreach_splitchr(bs7, BYTESTRING("  "), ' ') {
		n++;
		ok1(bs7.ptr && !bs7.len);
	}
	ok1(n == 3);

	n = 0;
	bytestring_foreach_splitchr(bs7, BYTESTRING("\0\0\0"), '\0') {
		n++;
		ok1(bs7.ptr && !bs7.len);
	}
	ok1(n == 4);

	bs7 = bytestring_splitchrs_first(bs, BYTESTRING(" \0"));
	ok1(bs7.ptr == bs.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("test")));
	bs7 = bytestring_splitchrs_next(bs, BYTESTRING(" \0"), bs7);
	ok1(bs7.ptr == bs.ptr + 5);
	ok1(bytestring_eq(bs7, BYTESTRING("string")));
	bs7 = bytestring_splitchrs_next(bs, BYTESTRING(" \0"), bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchrs_next(bs, BYTESTRING(" \0"), bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchrs_first(bs2, BYTESTRING(" \0"));
	ok1(bs7.ptr == bs2.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("abc")));
	bs7 = bytestring_splitchrs_next(bs2, BYTESTRING(" \0"), bs7);
	ok1(bs7.ptr == bs2.ptr + 4);
	ok1(bytestring_eq(bs7, BYTESTRING("def")));
	bs7 = bytestring_splitchrs_next(bs2, BYTESTRING(" \0"), bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitchrs_next(bs2, BYTESTRING(" \0"), bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitchrs_first(BYTESTRING(""), BYTESTRING(" \0"));
	ok1(!bs7.ptr);

	n = 0;
	bytestring_foreach_splitchrs(bs7, BYTESTRING(" \0 \0 "),
				     BYTESTRING("\0 ")) {
		n++;
		ok1(bs7.ptr && !bs7.len);
	}
	ok1(n == 6);

	bs7 = bytestring_splitstr_first(bs, BYTESTRING("t "));
	ok1(bs7.ptr == bs.ptr);
	ok1(bytestring_eq(bs7, BYTESTRING("tes")));
	bs7 = bytestring_splitstr_next(bs, BYTESTRING("t "), bs7);
	ok1(bs7.ptr == bs.ptr + 5);
	ok1(bytestring_eq(bs7, BYTESTRING("string")));
	bs7 = bytestring_splitstr_next(bs, BYTESTRING("t "), bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitstr_next(bs, BYTESTRING("t "), bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitstr_first(bs2, BYTESTRING("abc"));
	ok1(bs7.ptr == bs2.ptr);
	ok1(bs7.len == 0);
	bs7 = bytestring_splitstr_next(bs2, BYTESTRING("abc"), bs7);
	ok1(bs7.ptr == bs2.ptr + 3);
	ok1(bytestring_eq(bs7, BYTESTRING("\0def")));
	bs7 = bytestring_splitstr_next(bs2, BYTESTRING("t "), bs7);
	ok1(!bs7.ptr);
	bs7 = bytestring_splitstr_next(bs2, BYTESTRING("t "), bs7);
	ok1(!bs7.ptr);

	bs7 = bytestring_splitstr_first(BYTESTRING(""), BYTESTRING(""));
	ok1(!bs7.ptr);

	n = 0;
	bytestring_foreach_splitstr(bs7, BYTESTRING("foofoo"),
				     BYTESTRING("foo")) {
		n++;
		ok1(bs7.ptr && !bs7.len);
	}
	ok1(n == 3);
	
	/* This exits depending on whether all tests passed */
	return exit_status();
}
