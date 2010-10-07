#ifndef CCAN_STR_TALLOC_H
#define CCAN_STR_TALLOC_H
#include <string.h>
#include <stdbool.h>

/**
 * strsplit - Split string into an array of substrings
 * @ctx: the context to tallocate from (often NULL)
 * @string: the string to split
 * @delims: delimiters where lines should be split.
 * @nump: optional pointer to place resulting number of lines
 *
 * This function splits a single string into multiple strings.  The
 * original string is untouched: an array is allocated (using talloc)
 * pointing to copies of each substring.  Multiple delimiters result
 * in empty substrings.  By definition, no delimiters will appear in
 * the substrings.
 *
 * The final char * in the array will be NULL, so you can use this or
 * @nump to find the array length.
 *
 * Example:
 *	#include <ccan/talloc/talloc.h>
 *	#include <ccan/str_talloc/str_talloc.h>
 *	...
 *	static unsigned int count_long_lines(const char *string)
 *	{
 *		char **lines;
 *		unsigned int i, long_lines = 0;
 *
 *		// Can only fail on out-of-memory.
 *		lines = strsplit(NULL, string, "\n", NULL);
 *		for (i = 0; lines[i] != NULL; i++)
 *			if (strlen(lines[i]) > 80)
 *				long_lines++;
 *		talloc_free(lines);
 *		return long_lines;
 *	}
 */
char **strsplit(const void *ctx, const char *string, const char *delims,
		 unsigned int *nump);

/**
 * strjoin - Join an array of substrings into one long string
 * @ctx: the context to tallocate from (often NULL)
 * @strings: the NULL-terminated array of strings to join
 * @delim: the delimiter to insert between the strings
 *
 * This function joins an array of strings into a single string.  The
 * return value is allocated using talloc.  Each string in @strings is
 * followed by a copy of @delim.
 *
 * Example:
 *	// Append the string "--EOL" to each line.
 *	static char *append_to_all_lines(const char *string)
 *	{
 *		char **lines, *ret;
 *
 *		lines = strsplit(NULL, string, "\n", NULL);
 *		ret = strjoin(NULL, lines, "-- EOL\n");
 *		talloc_free(lines);
 *		return ret;
 *	}
 */
char *strjoin(const void *ctx, char *strings[], const char *delim);
#endif /* CCAN_STR_TALLOC_H */
