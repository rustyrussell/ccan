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
	plan_tests(9);

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

	/* This exits depending on whether all tests passed */
	return exit_status();
}
