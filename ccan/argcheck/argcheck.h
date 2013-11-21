/*****************************************************************************
 *
 * argcheck - macros for argument value checking
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 ****************************************************************************/

#ifndef CCAN_ARGCHECK_H
#define CCAN_ARGCHECK_H
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <ccan/likely/likely.h>
#include <ccan/compiler/compiler.h>

/**
 * ARGCHECK_DISABLE_LOGGING - define to disable any logging done by
 * argcheck. This does not disable the actual checks, merely the invocation
 * of the argcheck_log function, be it custom or the default logging
 * function.
 */
#ifdef ARGCHECK_DISABLE_LOGGING
#undef argcheck_log
#define argcheck_log(...) /* __VA_ARGS__ */
#else
#ifndef argcheck_log
/**
 * argcheck_log - logging function for failed argcheck tests
 *
 * Override this function to hook up your own logging function. The function
 * will be called once for each failed test.
 */
#define argcheck_log argcheck_log_
#endif /* argcheck_log */

static inline void COLD PRINTF_FMT(4, 5)
argcheck_log_(const char *file, int line, const char *func,
	      const char *msg, ...)
{
	va_list args;
	va_start(args, msg);
	fprintf(stderr, "argcheck: %s:%d %s\nargcheck: ", file, line, func);
	vfprintf(stderr, msg, args);
	va_end(args);
}
#endif

/**
 * argcheck_int_eq - check the argument is equal to the value.
 */
#define argcheck_int_eq(arg, val) \
	argcheck_int_eq_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_ne - check the argument is not equal to the value
 */
#define argcheck_int_ne(arg, val) \
	argcheck_int_ne_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_ge - check the argument is equal or greater than the value
 */
#define argcheck_int_ge(arg, val) \
	argcheck_int_ge_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_gt - check the argument is greater than the value
 */
#define argcheck_int_gt(arg, val) \
	argcheck_int_gt_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_le - check the argument is equal or less than the value
 */
#define argcheck_int_le(arg, val) \
	argcheck_int_le_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_lt - check the argument is less than the value
 */
#define argcheck_int_lt(arg, val) \
	argcheck_int_lt_(arg, val, #arg, #val, __FILE__, __LINE__, __func__)
/**
 * argcheck_int_range - check the argument is within a range (inclusive)
 */
#define argcheck_int_range(arg, min, max) \
	argcheck_int_range_(arg, min, max, #arg, #min, #max, __FILE__, __LINE__, __func__)
/**
 * argcheck_flag_set - check if a flag is set
 */
#define argcheck_flag_set(arg, flag) \
	argcheck_flag_set_(arg, flag, #arg, #flag, __FILE__, __LINE__, __func__)
/**
 * argcheck_flag_unset - check if a flag is not set
 */
#define argcheck_flag_unset(arg, flag) \
	argcheck_flag_unset_(arg, flag, #arg, #flag, __FILE__, __LINE__, __func__)
/**
 * argcheck_ptr_not_null - check that a pointer is not NULL
 */
#define argcheck_ptr_not_null(arg) \
	argcheck_ptr_not_null_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_ptr_null - check that a pointer is NULL
 */
#define argcheck_ptr_null(arg) \
	argcheck_ptr_null_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_null - see argcheck_ptr_null
 */
#define argcheck_str_null(arg) \
	argcheck_str_null_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_not_null - see argcheck_ptr_not_null
 */
#define argcheck_str_not_null(arg) \
	argcheck_str_not_null_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_zero - check that a string is not NULL and of zero length
 */
#define argcheck_str_zero_len(arg) \
	argcheck_str_zero_len_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_null_or_zero - check that a string is either NULL or of zero length
 */
#define argcheck_str_null_or_zero_len(arg) \
	argcheck_str_null_or_zero_len_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_not_zero - check that a string is not NULL and has a length greater than 0
 */
#define argcheck_str_not_zero_len(arg) \
	argcheck_str_not_zero_len_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_null_or_not_zero - check that a string is either NULL or has a length greater than 0
 */
#define argcheck_str_null_or_not_zero_len(arg) \
	argcheck_str_null_or_not_zero_len_(arg, #arg, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_min_len - check that a string is not NULL and has a length greater than or equal to a minimum
 */
#define argcheck_str_min_len(arg, min) \
	argcheck_str_min_len_(arg, #arg, min, #min, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_max_len - check that a string is not NULL and has a length less than or equal to a maximum
 */
#define argcheck_str_max_len(arg, max) \
	argcheck_str_max_len_(arg, #arg, max, #max, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_null_or_min_len - check that a string is NULL or has a length greater than or equal to a minimum
 */
#define argcheck_str_null_or_min_len(arg, min) \
	argcheck_str_null_or_min_len_(arg, #arg, min, #min, __FILE__, __LINE__, __func__)
/**
 * argcheck_str_null_or_max_len - check that a string is NULL or has a length less than or equal to a maximum
 */
#define argcheck_str_null_or_max_len(arg, max) \
	argcheck_str_null_or_max_len_(arg, #arg, max, #max, __FILE__, __LINE__, __func__)

/*

   below is the actual implemenation. do not use it directly, use the macros
   above instead

 */

static inline int argcheck_int_eq_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a == b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s == %s)\" (%d == %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_ne_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a != b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s != %s)\" (%d != %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_ge_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a >= b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s >= %s)\" (%d >= %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_gt_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a > b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s > %s)\" (%d > %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_le_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a <= b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s <= %s)\" (%d <= %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_lt_(int a, int b, const char *astr, const char *bstr,
				   const char *file, int line, const char *func)
{
	if (likely(a < b))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s < %s)\" (%d < %d) failed\n", astr, bstr, a, b);
	return 0;
}

static inline int argcheck_int_range_(int v, int min,  int max,
				      const char *vstr,
				      const char *minstr, const char *maxstr,
				      const char *file, int line, const char *func)
{
	if (!argcheck_int_le_(min, max, minstr, maxstr, file, line, func))
		return 0;

	if (likely(v >= min && v <= max))
		return 1;

	argcheck_log(file, line, func,
		     "condition \"(%s <= %s <= %s)\" (%d <= %d <= %d) failed\n",
		     minstr, vstr, maxstr, min, v, max);
	return 0;
}

static inline int argcheck_flag_set_(int arg, int flag,
				     const char *argstr, const char *flagstr,
				     const char *file, int line, const char *func)
{
	if (!argcheck_int_ne_(flag, 0, flagstr, "0", file, line, func))
		return 0;

	if (likely(arg & flag))
		return 1;

	argcheck_log(file, line, func,
		     "flag \"%s\" (%d) is not set on \"%s\" (%d)\"\n",
		     flagstr, flag, argstr, arg);
	return 0;
}

static inline int argcheck_flag_unset_(int arg, int flag,
				       const char *argstr, const char *flagstr,
				       const char *file, int line, const char *func)
{
	if (!argcheck_int_ne_(flag, 0, flagstr, "0", file, line, func))
		return 0;

	if (likely((arg & flag) == 0))
		return 1;

	argcheck_log(file, line, func,
		     "flag \"%s\" (%d) must not be set on \"%s\" (%d)\"\n",
		     flagstr, flag, argstr, arg);
	return 0;
}

static inline int argcheck_ptr_not_null_(const void *arg, const char *argstr,
				        const char *file, int line, const char *func)
{
	if (likely(arg != NULL))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must not be NULL\n", argstr);
	return 0;
}

static inline int argcheck_ptr_null_(const void *arg, const char *argstr,
				     const char *file, int line, const char *func)
{
	if (likely(arg == NULL))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be NULL\n", argstr);
	return 0;
}

static inline int argcheck_str_null_(const char *arg, const char *argstr,
				     const char *file, int line, const char *func)
{
	return argcheck_ptr_null_(arg, argstr, file, line, func);
}

static inline int argcheck_str_not_null_(const char *arg, const char *argstr,
				         const char *file, int line, const char *func)
{
	return argcheck_ptr_not_null_(arg, argstr, file, line, func);
}

static inline int argcheck_str_zero_len_(const char *arg, const char *argstr,
				         const char *file, int line, const char *func)
{
	if (!argcheck_str_not_null_(arg, argstr, file, line, func))
		return 0;

	if (likely(*arg == '\0'))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be a zero-length string\n", argstr);
	return 0;
}


static inline int argcheck_str_null_or_zero_len_(const char *arg, const char *argstr,
						 const char *file, int line, const char *func)
{
	if (likely(arg == NULL || *arg == '\0'))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be NULL or a zero-length string\n", argstr);
	return 0;
}

static inline int argcheck_str_not_zero_len_(const char *arg, const char *argstr,
					     const char *file, int line, const char *func)
{
	if (!argcheck_str_not_null_(arg, argstr, file, line, func))
		return 0;

	if (likely(*arg != '\0'))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must not be a zero-length string\n", argstr);
	return 0;
}

static inline int argcheck_str_null_or_not_zero_len_(const char *arg, const char *argstr,
						     const char *file, int line, const char *func)
{
	if (likely(arg == NULL || *arg != '\0'))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be NULL or not a zero-length string\n", argstr);
	return 0;
}

static inline int argcheck_str_min_len_(const char *arg, const char *argstr,
					int min, const char *minstr,
					const char *file, int line, const char *func)
{
	int len;

	if (!argcheck_str_not_null_(arg, argstr, file, line, func))
		return 0;

	len = strlen(arg);
	if (likely(len >= min))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be at least \"%s\" (%d) long, is '%s' (length %d)\n", argstr,
		     minstr, min, arg, len);
	return 0;
}

static inline int argcheck_str_max_len_(const char *arg, const char *argstr,
					int max, const char *maxstr,
					const char *file, int line, const char *func)
{
	int len;

	if (!argcheck_str_not_null_(arg, argstr, file, line, func))
		return 0;

	len = strlen(arg);
	if (likely(len <= max))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be at most \"%s\" (%d) long, is '%s' (length %d)\n", argstr,
		     maxstr, max, arg, len);
	return 0;
}

static inline int argcheck_str_null_or_min_len_(const char *arg, const char *argstr,
						int min, const char *minstr,
						const char *file, int line, const char *func)
{
	int len;

	if (likely(arg == NULL))
		return 1;

	len = strlen(arg);
	if (likely(len >= min))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be NULL or at least \"%s\" (%d) long, is '%s' (length %d)\n", argstr,
		     minstr, min, arg, len);
	return 0;
}

static inline int argcheck_str_null_or_max_len_(const char *arg, const char *argstr,
						int max, const char *maxstr,
						const char *file, int line, const char *func)
{
	int len;

	if (likely(arg == NULL))
		return 1;

	len = strlen(arg);
	if (likely(len <= max))
		return 1;

	argcheck_log(file, line, func,
		     "\"%s\" must be NULL or at most \"%s\" (%d) long, is '%s' (length %d)\n", argstr,
		     maxstr, max, arg, len);
	return 0;
}

#endif /* CCAN_ARGCHECK_H */
