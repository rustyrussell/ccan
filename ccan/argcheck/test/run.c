#include <ccan/tap/tap.h>
#include <stdarg.h>
#include <stdio.h>

#include <ccan/argcheck/argcheck.h>

int main(void)
{
	int a = 0;
	const int flag = 0x1;
	const int invalid_flag = 0x0;
	int *ptr = NULL,
	    *ptr_not_null = &a;

	const char *str = "hello",
	           *str_zero = "\0",
		   *str_null = NULL;

	plan_tests(60);

	ok1(!argcheck_int_eq(a, 1));
	ok1(argcheck_int_eq(a, 0));

	ok1(!argcheck_int_ne(a, 0));
	ok1(argcheck_int_ne(a, 10));

	ok1(!argcheck_int_ge(a, 1));
	ok1(argcheck_int_ge(a, 0));
	ok1(argcheck_int_ge(a, -1));

	ok1(!argcheck_int_gt(a, 1));
	ok1(!argcheck_int_gt(a, 0));
	ok1(argcheck_int_gt(a, -1));

	ok1(!argcheck_int_le(a, -1));
	ok1(argcheck_int_le(a, 0));
	ok1(argcheck_int_le(a, 1));

	ok1(!argcheck_int_lt(a, -1));
	ok1(!argcheck_int_lt(a, 0));
	ok1(argcheck_int_lt(a, 1));

	ok1(!argcheck_int_range(a, 0, -1));
	ok1(!argcheck_int_range(a, -3, -1));
	ok1(argcheck_int_range(a, 0, 1));
	ok1(argcheck_int_range(a, -1, 0));

	ok1(!argcheck_flag_set(a, invalid_flag));
	ok1(!argcheck_flag_set(a, flag));
	ok1(argcheck_flag_set(a | flag, flag));

	ok1(!argcheck_flag_unset(a, invalid_flag));
	ok1(!argcheck_flag_unset(a | flag, flag));
	ok1(argcheck_flag_unset(a, flag));

	ok1(argcheck_ptr_null(ptr));
	ok1(!argcheck_ptr_not_null(ptr));
	ok1(!argcheck_ptr_null(ptr_not_null));
	ok1(argcheck_ptr_not_null(ptr_not_null));

	ok1(argcheck_str_null(str_null));
	ok1(!argcheck_str_not_null(str_null));
	ok1(!argcheck_str_null(str));
	ok1(argcheck_str_not_null(str));
	ok1(!argcheck_str_null(str_zero));
	ok1(argcheck_str_not_null(str_zero));

	ok1(!argcheck_str_zero_len(str_null));
	ok1(argcheck_str_zero_len(str_zero));
	ok1(!argcheck_str_zero_len(str));

	ok1(!argcheck_str_not_zero_len(str_null));
	ok1(!argcheck_str_not_zero_len(str_zero));
	ok1(argcheck_str_not_zero_len(str));

	ok1(!argcheck_str_min_len(str_null, 1));
	ok1(!argcheck_str_min_len(str_zero, 1));
	ok1(argcheck_str_min_len(str, 1));

	ok1(!argcheck_str_max_len(str_null, 1));
	ok1(argcheck_str_max_len(str_zero, 1));
	ok1(!argcheck_str_max_len(str, 1));

	ok1(argcheck_str_null_or_zero_len(str_null));
	ok1(argcheck_str_null_or_zero_len(str_zero));
	ok1(!argcheck_str_null_or_zero_len(str));

	ok1(argcheck_str_null_or_not_zero_len(str_null));
	ok1(!argcheck_str_null_or_not_zero_len(str_zero));
	ok1(argcheck_str_null_or_not_zero_len(str));

	ok1(argcheck_str_null_or_min_len(str_null, 1));
	ok1(!argcheck_str_null_or_min_len(str_zero, 1));
	ok1(argcheck_str_null_or_min_len(str, 1));

	ok1(argcheck_str_null_or_max_len(str_null, 1));
	ok1(argcheck_str_null_or_max_len(str_zero, 1));
	ok1(!argcheck_str_null_or_max_len(str, 1));

	return exit_status();
}
