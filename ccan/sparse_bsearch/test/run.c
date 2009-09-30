/* Include the main header first, to test it works */
#include <ccan/sparse_bsearch/sparse_bsearch.h>
/* Include the C files directly. */
#include <ccan/sparse_bsearch/sparse_bsearch.c>
#include <ccan/tap/tap.h>

static int cmp(const unsigned int *a, const unsigned int *b)
{
	return (*a) - (*b);
}

static bool valid(const unsigned int *a)
{
	return *a != 0;
}

int main(void)
{
	unsigned int i, j, master[4] = { 1, 2, 3, 4 }, arr[4];

	plan_tests((1 << 4) * 4+ (1 << 3) * 3);

	/* We test all possibilities of an even and an odd array len. */
	for (i = 0; i < (1 << 4); i++) {
		/* Setup partial arr[] */
		for (j = 0; j < 4; j++) {
			if (i & (1 << j))
				arr[j] = master[j];
			else
				arr[j] = 0;
		}

		for (j = 1; j <= 4; j++) {
			unsigned int *ptr;
			ptr = sparse_bsearch(&j, arr, 4, cmp, valid);
			if (i & (1 << (j-1)))
				ok1(ptr && *ptr == j);
			else
				ok1(!ptr);
		}
	}

	for (i = 0; i < (1 << 3); i++) {
		/* Setup partial arr[] */
		for (j = 0; j < 3; j++) {
			if (i & (1 << j))
				arr[j] = master[j];
			else
				arr[j] = 0;
		}

		for (j = 1; j <= 3; j++) {
			unsigned int *ptr;
			ptr = sparse_bsearch(&j, arr, 3, cmp, valid);
			if (i & (1 << (j-1)))
				ok1(ptr && *ptr == j);
			else
				ok1(!ptr);
		}
	}

	/* This exits depending on whether all tests passed */
	return exit_status();
}
