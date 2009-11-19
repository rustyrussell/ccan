#include <ccan/asearch/asearch.h>
#include <ccan/array_size/array_size.h>
#include <ccan/tap/tap.h>
#include <limits.h>

static int test_cmp(const int *key, const int *elt)
{
	if (*key < *elt)
		return -1;
	else if (*key > *elt)
		return 1;
	return 0;
}

int main(void)
{
	const int arr[] = { INT_MIN, 0, 1, 2, 3, 4, 5, 6, INT_MAX };
	unsigned int start, num, i, total = 0;
	int key;

	plan_tests(285);

	for (start = 0; start < ARRAY_SIZE(arr); start++) {
		for (num = 0; num < ARRAY_SIZE(arr) - start; num++) {
			key = 7;
			ok1(asearch(&key, &arr[start], num, test_cmp) == NULL);
			total++;
			for (i = start; i < start+num; i++) {
				const int *ret;
				key = arr[i];
				ret = asearch(&key, &arr[start], num, test_cmp);
				ok1(ret);
				ok1(ret && *ret == key);
				total++;
			}
		}
	}
	diag("Tested %u searches\n", total);
	return exit_status();
}
