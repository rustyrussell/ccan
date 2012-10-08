/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_BYTESTRING_H_
#define CCAN_BYTESTRING_H_

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <ccan/array_size/array_size.h>

struct bytestring {
	const char *ptr;
	size_t len;
};

/**
 * bytestring - construct a new bytestring
 * @p: pointer to the content of the bytestring
 * @l: length of the bytestring
 *
 * Builds a new bytestring starting at p, of length l.
 *
 * Example:
 *	char x[5] = "abcde";
 *	struct bytestring bs = bytestring(x, 5);
 *	assert(bs.len == 5);
 */
static inline struct bytestring bytestring(const char *p, size_t l)
{
	struct bytestring bs = {
		.ptr = p,
		.len = l,
	};

	return bs;
}

#define bytestring_NULL		bytestring(NULL, 0)

/**
 * BYTESTRING - construct a bytestring from a string literal
 * @s: string literal
 *
 * Builds a new bytestring containing the given literal string, not
 * including the terminating \0 (but including any internal \0s).
 *
 * Example:
 *	assert(BYTESTRING("abc\0def").len == 7);
 */
#define BYTESTRING(s) (bytestring((s), ARRAY_SIZE(s) - 1))


/**
 * bytestring_from_string - construct a bytestring from a NUL terminated string
 * @s: NUL-terminated string pointer
 *
 * Builds a new bytestring containing the given NUL-terminated string,
 * up to, but not including, the terminating \0.
 *
 * Example:
 *	assert(bytestring_from_string("abc\0def").len == 3);
 */
static inline struct bytestring bytestring_from_string(const char *s)
{
	if (!s)
		return bytestring_NULL;
	return bytestring(s, strlen(s));
}

/**
 * bytestring_eq - test if bytestrings have identical content
 * @a, @b: bytestrings
 *
 * Returns 1 if the given bytestrings have identical length and
 * content, 0 otherwise.
 */
static inline bool bytestring_eq(struct bytestring a, struct bytestring b)
{
	return (a.len == b.len)
		&& (memcmp(a.ptr, b.ptr, a.len) == 0);
}

#endif /* CCAN_BYTESTRING_H_ */
