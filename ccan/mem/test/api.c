#include <ccan/array_size/array_size.h>
#include <ccan/mem/mem.h>
#include <ccan/tap/tap.h>

int main(void)
{
	char haystack1[] = "abcd\0efgh";
	char haystack2[] = "ab\0ab\0ab\0ab";
	char needle1[] = "ab";
	char needle2[] = "d\0e";

	/* This is how many tests you plan to run */
	plan_tests(19);

	ok1(memmem(haystack1, sizeof(haystack1), needle1, 2) == haystack1);
	ok1(memmem(haystack1, sizeof(haystack1), needle1, 3) == NULL);
	ok1(memmem(haystack1, sizeof(haystack1), needle2, 3) == (haystack1 + 3));

	ok1(memmem(haystack2, sizeof(haystack2), needle1, sizeof(needle1))
	    == haystack2);
	ok1(memmem(haystack2, sizeof(haystack2), needle2, 3) == NULL);

	ok1(memrchr(haystack1, 'a', sizeof(haystack1)) == haystack1);
	ok1(memrchr(haystack1, 'b', sizeof(haystack1)) == haystack1 + 1);
	ok1(memrchr(haystack1, 'c', sizeof(haystack1)) == haystack1 + 2);
	ok1(memrchr(haystack1, 'd', sizeof(haystack1)) == haystack1 + 3);
	ok1(memrchr(haystack1, 'e', sizeof(haystack1)) == haystack1 + 5);
	ok1(memrchr(haystack1, 'f', sizeof(haystack1)) == haystack1 + 6);
	ok1(memrchr(haystack1, 'g', sizeof(haystack1)) == haystack1 + 7);
	ok1(memrchr(haystack1, 'h', sizeof(haystack1)) == haystack1 + 8);
	ok1(memrchr(haystack1, '\0', sizeof(haystack1)) == haystack1 + 9);
	ok1(memrchr(haystack1, 'i', sizeof(haystack1)) == NULL);

	ok1(memrchr(haystack2, 'a', sizeof(haystack2)) == haystack2 + 9);
	ok1(memrchr(haystack2, 'b', sizeof(haystack2)) == haystack2 + 10);
	ok1(memrchr(haystack2, '\0', sizeof(haystack2)) == haystack2 + 11);

	ok1(memrchr(needle1, '\0', 2) == NULL);

	/* This exits depending on whether all tests passed */
	return exit_status();
}
