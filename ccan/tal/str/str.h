/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_STR_TAL_H
#define CCAN_STR_TAL_H
#include <ccan/tal/tal.h>
#include <ccan/tal/tal.h>
#include <string.h>
#include <stdbool.h>

enum strsplit {
	STR_EMPTY_OK,
	STR_NO_EMPTY
};

/**
 * strsplit - Split string into an array of substrings
 * @ctx: the context to tal from (often NULL).
 * @string: the string to split (can be take()).
 * @delims: delimiters where lines should be split (can be take()).
 * @flags: whether to include empty substrings.
 *
 * This function splits a single string into multiple strings.
 *
 * If @string is take(), the returned array will point into the
 * mangled @string.
 *
 * Multiple delimiters result in empty substrings.  By definition, no
 * delimiters will appear in the substrings.
 *
 * The final char * in the array will be NULL.
 *
 * Example:
 *	#include <ccan/tal/str/str.h>
 *	...
 *	static unsigned int count_long_lines(const char *string)
 *	{
 *		char **lines;
 *		unsigned int i, long_lines = 0;
 *
 *		// Can only fail on out-of-memory.
 *		lines = strsplit(NULL, string, "\n", STR_NO_EMPTY);
 *		for (i = 0; lines[i] != NULL; i++)
 *			if (strlen(lines[i]) > 80)
 *				long_lines++;
 *		tal_free(lines);
 *		return long_lines;
 *	}
 */
char **strsplit(const tal_t *ctx,
		const char *string, const char *delims, enum strsplit flags);

enum strjoin {
	STR_TRAIL,
	STR_NO_TRAIL
};

/**
 * strjoin - Join an array of substrings into one long string
 * @ctx: the context to tal from (often NULL).
 * @strings: the NULL-terminated array of strings to join (can be take())
 * @delim: the delimiter to insert between the strings (can be take())
 * @flags: whether to add a delimieter to the end
 *
 * This function joins an array of strings into a single string.  The
 * return value is allocated using tal.  Each string in @strings is
 * followed by a copy of @delim.
 *
 * Example:
 *	// Append the string "--EOL" to each line.
 *	static char *append_to_all_lines(const char *string)
 *	{
 *		char **lines, *ret;
 *
 *		lines = strsplit(NULL, string, "\n", STR_EMPTY_OK);
 *		ret = strjoin(NULL, lines, "-- EOL\n", STR_TRAIL);
 *		tal_free(lines);
 *		return ret;
 *	}
 */
char *strjoin(const void *ctx, char *strings[], const char *delim,
	      enum strjoin flags);

/**
 * strreg - match and extract from a string via (extended) regular expressions.
 * @ctx: the context to tal from (often NULL)
 * @string: the string to try to match (can be take())
 * @regex: the regular expression to match (can be take())
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
 *		input = strjoin(NULL, argv+1, " ", STR_NO_TRAIL);
 *		if (strreg(NULL, input, "[Mm]y (first )?name is ([A-Za-z ]+)",
 *			   NULL, &person))
 *			printf("Hello %s!\n", person);
 *		else
 *			printf("Hello there!\n");
 *		return 0;
 *	}
 */
bool strreg(const void *ctx, const char *string, const char *regex, ...);
#endif /* CCAN_STR_TAL_H */
