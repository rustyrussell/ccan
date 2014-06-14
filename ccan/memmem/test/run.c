#include <ccan/array_size/array_size.h>
#include <ccan/memmem/memmem.h>
#include <ccan/tap/tap.h>

int main(void)
{
	char haystack1[] = "abcd\0efgh";
	char needle1[] = "ab";
	char needle2[] = "d\0e";

	/* This is how many tests you plan to run */
	plan_tests(3);

	ok1(memmem(haystack1, sizeof(haystack1), needle1, 2) == haystack1);
	ok1(memmem(haystack1, sizeof(haystack1), needle1, 3) == NULL);
	ok1(memmem(haystack1, sizeof(haystack1), needle2, 3) == (haystack1 + 3));

	/* This exits depending on whether all tests passed */
	return exit_status();
}
