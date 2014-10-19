#include <ccan/bytestring/bytestring.h>
#include <ccan/tap/tap.h>

#define TEST_STRING	"test string"
#define TEST_STRING_2	"abc\0def"

const char str1[] = TEST_STRING;
const char *str2 = TEST_STRING;

int main(void)
{
	struct bytestring bs, bs1, bs2, bs3, bs4, bs5;

	/* This is how many tests you plan to run */
	plan_tests(30);

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

	/* This exits depending on whether all tests passed */
	return exit_status();
}
