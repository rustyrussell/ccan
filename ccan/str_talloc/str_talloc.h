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

/**
 * strreg - match and extract from a string via (extended) regular expressions.
 * @ctx: the context to tallocate from (often NULL)
 * @string: the string to try to match.
 * @regex: the regular expression to match.
 * ...: pointers to strings to allocate for subexpressions.
 *
 * Returns true if we matched, in which case any parenthesized
 * expressions in @regex are allocated and placed in the char **
 * arguments following @regex.  NULL arguments mean the match is not
 * saved.  The order of the strings is the order
 * of opening braces in the expression: in the case of repeated
 * expressions (eg "([a-z])*") the last one is saved, in the case of
 * non-existent matches (eg "([a-z]*)?") the pointer is set to NULL.
 *
 * Allocation failures or malformed regular expressions return false.
 *
 * See Also:
 *	regcomp(3), regex(3).
 *
 * Example:
 *	// Given 'My name is Rusty' outputs 'Hello Rusty!'
 *	// Given 'my first name is Rusty Russell' outputs 'Hello Rusty Russell!'
 *	// Given 'My name isnt Rusty Russell' outputs 'Hello there!'
 *	int main(int argc, char *argv[])
 *	{
 *		char *person, *input;
 *
 *		// Join args and trim trailing space.
 *		input = strjoin(NULL, argv+1, " ");
 *		if (strlen(input) != 0)
 *			input[strlen(input)-1] = '\0';
 *
 *		if (strreg(NULL, input, "[Mm]y (first )?name is ([A-Za-z ]+)",
 *			   NULL, &person))
 *			printf("Hello %s!\n", person);
 *		else
 *			printf("Hello there!\n");
 *		return 0;
 *	}
 */
bool strreg(const void *ctx, const char *string, const char *regex, ...);
#endif /* CCAN_STR_TALLOC_H */
