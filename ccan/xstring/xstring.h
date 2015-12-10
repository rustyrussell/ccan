#ifndef _XSTRING_H
#define _XSTRING_H 1

#include <assert.h>
#include <sys/types.h>

/**
 * struct xstring - string metadata
 * @str: pointer to buf
 * @len: current length of buf contents
 * @cap: maximum capacity of buf
 * @truncated: -1 indicates truncation
 */
typedef struct xstring {
	char *str;
	size_t len;
	size_t cap;
	int truncated;
} xstring;

/**
 * xstrNew - mallocs and inits
 * @size: size of buffer to allocate
 *
 * mallocs both buffer and struct, calls xstrInit
 *
 * Return: ptr to xstring or NULL
 */
xstring *xstrNew(const size_t size);

/**
 * xstrInit - initialize an xstring struct
 * @x: pointer to xstring
 * @str: buffer to manage
 * @size: size of str
 * @keep: if !0, keep existing contents of str;
 *        otherwise, str[0] = '\0'
 *
 * Return: x->truncated
 */
int xstrInit(xstring *x, char *str, const size_t size, int keep);

/**
 * xstrClear - clear xstring
 * @x: pointer to xstring
 *
 * This sets x->len and x->truncated to 0 and str[0] to '\0';
 *
 * Return: x->truncated (always 0)
 */
#define xstrClear(x) (xstrInit((x), (x)->str, (x)->cap, 0))

/**
 * xstrAddChar - add a single character
 * @x: pointer to xstring
 * @c: character to append
 *
 * Return: x->truncated
 */
int xstrAddChar(xstring *x, const char c);

/**
 * xstrAdd - append a string
 * @x: pointer to xstring
 * @src: string to append
 *
 * Append as much from src as fits - if not all, flag truncation.
 *
 * Return: x->truncated
 */
int xstrAdd(xstring *x, const char *src);

/**
 * xstrAddSub - append a substring
 * @x: pointer to xstring
 * @src: string to append
 * @len: length of substring
 *
 * Append substring and '\0' if len fits, otherwise flag truncation.
 *
 * Return: x->truncated
 */
int xstrAddSub(xstring *x, const char *src, size_t len);

/** xstrCat - append multiple strings
 * @x: pointer to xstring
 * @...: one or more strings followed by NULL
 *
 * Run xstrAdd for each string.
 *
 * Return: x->truncated
 */
int xstrCat(xstring *x, ...);

/** xstrJoin - append multiple strings joined by sep
 * @x: pointer to xstring
 * @sep: separator string
 * @...: one or more strings followed by NULL
 *
 * Run xstrAdd for each string and append sep between each pair.
 *
 * Return: x->truncated
 */
int xstrJoin(xstring *x, const char *sep, ...);

/**
 * xstrAddSubs - append multiple substrings
 * @x: pointer to xstring
 * @...: one or more pairs of string and length followed by NULL
 *
 * Run xstrAddSub for each pair of string and length.
 *
 * Return: x->truncated
 */
int xstrAddSubs(xstring *x, ...);

#define transact(x, y)  ({ \
	size_t last; \
	int ret; \
	assert((x)); \
	last = (x)->len; \
	if ((ret = (y)) == -1) { \
		(x)->len = last; \
		*((x)->str + (x)->len) = '\0'; \
	} \
	ret; \
})

/**
 * xstrAddT - append a string as a transaction
 * @x: pointer to xstring
 * @src: string to append
 *
 * Run xstrAdd. Reterminate at inital length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrAddT(x, y)        transact(x, xstrAdd((x), y))

/** xstrCatT - append multiple strings as one transaction
 * @x: pointer to xstring
 * @...: one or more strings followed by NULL
 *
 * Run xstrCat. Reterminate at inital length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrCatT(x, y...)     transact(x, xstrCat((x) , ## y))

/** xstrJoinT - append multiple strings joined by sep as one transaction
 * @x: pointer to xstring
 * @sep: separator string
 * @...: one or more strings followed by NULL
 *
 * Run xstrJoin. Reterminate at inital length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrJoinT(x, y...)    transact(x, xstrJoin((x) , ## y))

/**
 * xstrAddSubsT - append multiple substrings as one transaction
 * @x: pointer to xstring
 * @...: one or more pairs of string and length followed by NULL
 *
 * Run xstrAddSubs. Reterminate at inital length if truncation occurs.
 *
 * Return: x->truncated
 */
#define xstrAddSubsT(x, y...) transact(x, xstrAddSubs((x) , ## y))

/**
 * xstrAddSubT - same as xstrAddSub
 */
// addsub is already transactional
#define xstrAddSubT xstrAddSub

#define unknown -1

/**
 * xstrEq3 - test if two strings are equal
 * @x: pointer to xstring
 * @y: pointer to xstring
 *
 * Return true (1) if the strings held by x and y match and no truncation has occurred.
 * Return unknown (-1) if either is flagged as truncated.
 * Return false (0) otherwise.
 *
 * Return: -1, 0, or 1
 */
int xstrEq3(xstring *x, xstring *y);

/**
 * xstrEq - test if two strings are equal
 * @x: pointer to xstring
 * @y: pointer to xstring
 *
 * Same as xstrEq3, but return false (0) for unknown (-1).
 *
 * Return: 0 or 1
 */
#define xstrEq(x, y) (xstrEq3((x), (y)) == 1)

/**
 * xstrContains3 - test if first string contains second
 * @x: pointer to xstring
 * @y: pointer to xstring
 * @where: -1 (ends), 0 (anywhere), 1 (begins)
 *
 * If neither x nor y are truncated, returns true (1) or false (0).
 * If y is truncated, return unknown (-1).
 * If x is truncated, return true (1) if known portion of x contains y, unknown (-1) otherwise.
 *
 * Return: -1, 0, or 1
 */
int xstrContains3(xstring *x, xstring *y, int where);

/**
 * xstrContains3 - test if first string contains second
 * @x: pointer to xstring
 * @y: pointer to xstring
 * @where: -1 (ends), 0 (anywhere), 1 (begins)
 *
 * Same as xstrContains3, but return false (0) for unknown (-1).
 *
 * Return: 0 or 1
 */
#define xstrContains(x, y, w) (xstrContains3((x), (y), (w)) == 1)

/**
 * xstrFree - free malloced memory
 * @x: pointer to xstring
 */
void xstrFree(xstring *x);

#endif
