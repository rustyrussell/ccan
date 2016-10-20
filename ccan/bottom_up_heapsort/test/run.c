#include <ccan/bottom_up_heapsort/bottom_up_heapsort.h>
#include <ccan/bottom_up_heapsort/bottom_up_heapsort.c>
#include <ccan/array_size/array_size.h>
#include <ccan/tap/tap.h>
#include <limits.h>
#include <stdbool.h>

static int test_cmp( const void *p1, const void *p2, void *arg)
{
    const int *i1 = (const int*)p1;
    const int *i2 = (const int*)p2;
    int *flag = (int *)arg;

    if (*i1 < *i2) return -1 * *flag;
    if (*i1 > *i2) return 1 * *flag;
    
    return 0;
}

static bool is_sorted(const int arr[], unsigned int size)
{
	unsigned int i;

	for (i = 1; i < size; i++)
		if (arr[i] < arr[i-1])
			return false;
	return true;
}

static bool is_reverse_sorted(const int arr[], unsigned int size)
{
	unsigned int i;

	for (i = 1; i < size; i++)
		if (arr[i] > arr[i-1])
			return false;
	return true;
}

static void psuedo_random_array(int arr[], unsigned int size)
{
	unsigned int i;

	for (i = 0; i < size; i++)
		arr[i] = i * (INT_MAX / 4 - 7);
}

#define TEST_SIZE 500

int main(void)
{
	int tmparr[TEST_SIZE];
	int multiplier = 1;
	int rc;

	plan_tests(9);

	psuedo_random_array(tmparr, TEST_SIZE);
	ok1(!is_sorted(tmparr, TEST_SIZE));
	ok1(!is_reverse_sorted(tmparr, TEST_SIZE));

	rc = bottom_up_heapsort(tmparr, TEST_SIZE, &test_cmp, &multiplier);
	ok1(!rc);
	ok1(is_sorted(tmparr, TEST_SIZE));

	psuedo_random_array(tmparr, TEST_SIZE);
	multiplier = -1;
	rc = bottom_up_heapsort(tmparr, TEST_SIZE, &test_cmp, &multiplier);
	ok1(!rc);
	ok1(is_reverse_sorted(tmparr, TEST_SIZE));

	ok1(0 == bottom_up_heapsort(tmparr, 1, &test_cmp, &multiplier));
	ok1(-1 == _bottom_up_heapsort(tmparr, 2, 0, &test_cmp, &multiplier));
	ok1(-2 == bottom_up_heapsort(tmparr, 2, NULL, &multiplier));
	
	return exit_status();
}
