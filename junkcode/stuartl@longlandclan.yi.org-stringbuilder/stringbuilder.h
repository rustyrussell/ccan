/* CC0 (Public domain) - see LICENSE file for details */
#ifndef CCAN_STRINGBUILDER_H
#define CCAN_STRINGBUILDER_H
#include "config.h"
#include <stdarg.h>
#include <sys/types.h>

/**
 * stringbuilder - Join strings from a varadic list.  The list of arguments
 * are all assumed to be of type const char*.  If the first argument is str,
 * then the contents of str are preserved and appended to.
 *
 * @str:	A pointer to a string buffer that will receive the result.
 * @str_sz:	Size of the buffer pointed to by str.
 * @delim:	A delimiter to separate the strings with, or NULL.
 *
 * Returns:	0 on success
 * 		EMSGSIZE if the resulting string would overflow the buffer.
 * 		If an overflow condition is detected, the buffer content is
 * 		NOT defined.
 *
 * Example:
 * 	int res;
 * 	char file_name[80];
 * 	res = stringbuilder(file_name, sizeof(file_name), "/",
 * 		"/var/lib/foo", "bar", "baz");
 * 	if (res)
 * 		printf("Failed to determine file name: %s",
 * 			strerror(res));
 * 	else
 * 		printf("File is at %s", file_name);
 */
#define stringbuilder(str, str_sz, delim, ...) 	\
	stringbuilder_args(str, str_sz, delim, __VA_ARGS__, NULL)
/**
 * stringbuilder_args - Join strings from a varadic list.  The list of
 * arguments are all assumed to be of type const char* and must end with a NULL.
 * If the first argument is str, then the contents of str are preserved and
 * appended to.
 *
 * @str:	A pointer to a string buffer that will receive the result.
 * @str_sz:	Size of the buffer pointed to by str.
 * @delim:	A delimiter to separate the strings with, or NULL.
 *
 * Returns:	0 on success
 * 		EMSGSIZE if the resulting string would overflow the buffer.
 * 		If an overflow condition is detected, the buffer content is
 * 		NOT defined.
 *
 * Example:
 * 	int res;
 * 	char file_name[80];
 * 	res = stringbuilder_args(file_name, sizeof(file_name), "/",
 * 		"/var/lib/foo", "bar", "baz",
 * 		NULL);
 * 	if (res)
 * 		printf("Failed to determine file name: %s",
 * 			strerror(res));
 * 	else
 * 		printf("File is at %s", file_name);
 */
int stringbuilder_args(char* str, size_t str_sz, const char* delim, ...);

/**
 * stringbuilder_va - Join strings from a varadic list.  The list of arguments
 * are all assumed to be of type const char* and must end with a NULL.  If the
 * first argument is str, then the contents of str are preserved and appended
 * to.
 *
 * @str:	A pointer to a string buffer that will receive the result.
 * @str_sz:	Size of the buffer pointed to by str.
 * @delim:	A delimiter to separate the strings with, or NULL.
 *
 * Returns:	0 on success
 * 		EMSGSIZE if the resulting string would overflow the buffer.
 * 		If an overflow condition is detected, the buffer content is
 * 		NOT defined.
 *
 * Example:
 * 	#include <ccan/stringbuilder/stringbuilder.h>
 * 	#include <stdarg.h>
 * 	#include <stdio.h>
 * 	#include <string.h>
 * 	#include <errno.h>
 *
 *	int my_stringbuilder(char* str, size_t str_sz,
 *		const char* delim, ...);
 *
 *	int my_stringbuilder(char* str, size_t str_sz,
 *		const char* delim, ...)
 *	{
 *		int res;
 *		va_list ap;
 *		va_start(ap, delim);
 *		res = stringbuilder_va(str, str_sz, delim, ap);
 *		va_end(ap);
 *		return res;
 *	}
 *
 *	int main(void) {
 *		char my_string[80];
 *		int res = my_stringbuilder(my_string,
 *			sizeof(my_string), " ", "foo", "bar", NULL);
 *		if (!res)
 *			printf("%s\n", my_string);
 *		return res ? 1 : 0;
 *	}
 */
int stringbuilder_va(char* str, size_t str_sz, const char* delim, va_list ap);

/**
 * stringbuilder_array - Join strings from an array of const char* pointers.
 *
 * @str:	A pointer to a string buffer that will receive the result.
 * @str_sz:	Size of the buffer pointed to by str.
 * @delim:	A delimiter to separate the strings with, or NULL.
 * @n_strings:	The number of strings to join.
 * @strings:	The array of strings to join.
 *
 * Returns:	0 on success
 * 		EMSGSIZE if the resulting string would overflow the buffer.
 * 		If an overflow condition is detected, the buffer content is
 * 		NOT defined.
 *
 * Example:
 * 	char my_args[128];
 * 	int res = stringbuilder_array(my_args, sizeof(my_args), ", ",
 * 		argc, (const char**)argv);
 * 	if (res)
 * 		printf("Failed to list arguments: %s",
 * 			strerror(res));
 * 	else
 * 		printf("My arguments were %s", my_args);
 */
int stringbuilder_array(char* str, size_t str_sz, const char* delim,
		size_t n_strings, const char** strings);

#endif /* CCAN_STRINGBUILDER_H */
