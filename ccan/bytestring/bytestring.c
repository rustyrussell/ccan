/* Licensed under LGPLv2+ - see LICENSE file for details */
#include "config.h"

#include <ccan/bytestring/bytestring.h>

size_t bytestring_spn(struct bytestring s, struct bytestring accept)
{
	size_t i;

	for (i = 0; i < s.len; i++)
		if (bytestring_index(accept, s.ptr[i]) == NULL)
			return i;

	return s.len;
}

size_t bytestring_cspn(struct bytestring s, struct bytestring reject)
{
	size_t i;

	for (i = 0; i < s.len; i++)
		if (bytestring_index(reject, s.ptr[i]) != NULL)
			return i;

	return s.len;
}

static struct bytestring _splitchr(struct bytestring whole, char delim,
				   size_t start)
{
	const char *p;

	assert(start <= whole.len);

	/* Check this first, in case memchr() is not safe with zero length */
	if (start == whole.len)
		return bytestring(whole.ptr + start, 0);

	p = memchr(whole.ptr + start, delim, whole.len - start);
	if (p)
		return bytestring_slice(whole, start, p - whole.ptr);
	else
		return bytestring_slice(whole, start, whole.len);
}

struct bytestring bytestring_splitchr_first(struct bytestring whole,
					    char delim)
{
	if (whole.len == 0)
		return bytestring_NULL;

	return _splitchr(whole, delim, 0);
}

struct bytestring bytestring_splitchr_next(struct bytestring whole,
					   char delim, struct bytestring prev)
{
	if (!prev.ptr)
		return bytestring_NULL;

	/* prev has to be a substring of whole */
	assert(prev.ptr >= whole.ptr);

	if ((prev.ptr + prev.len) == (whole.ptr + whole.len))
		return bytestring_NULL;

	return _splitchr(whole, delim, (prev.ptr - whole.ptr) + prev.len + 1);
}

static struct bytestring _splitchrs(struct bytestring whole,
				    struct bytestring delim, size_t start)
{
	struct bytestring remainder;
	size_t n;

	assert(start <= whole.len);

	remainder = bytestring_slice(whole, start, whole.len);
	n = bytestring_cspn(remainder, delim);
	return bytestring_slice(whole, start, start + n);
}

struct bytestring bytestring_splitchrs_first(struct bytestring whole,
					     struct bytestring delim)
{
	if (whole.len == 0)
		return bytestring_NULL;

	return _splitchrs(whole, delim, 0);
}

struct bytestring bytestring_splitchrs_next(struct bytestring whole,
					    struct bytestring delim,
					    struct bytestring prev)
{
	if (!prev.ptr)
		return bytestring_NULL;

	/* prev has to be a substring of whole */
	assert(prev.ptr >= whole.ptr);

	if ((prev.ptr + prev.len) == (whole.ptr + whole.len))
		return bytestring_NULL;

	return _splitchrs(whole, delim, (prev.ptr - whole.ptr) + prev.len + 1);
}

static struct bytestring _splitstr(struct bytestring whole,
				   struct bytestring delim, size_t start)
{
	struct bytestring remainder, nextdelim;

	assert(start <= whole.len);

	remainder = bytestring_slice(whole, start, whole.len);
	nextdelim = bytestring_bytestring(remainder, delim);
	if (nextdelim.ptr)
		return bytestring_slice(whole, start,
					nextdelim.ptr - whole.ptr);
	else
		return remainder;
}

struct bytestring bytestring_splitstr_first(struct bytestring whole,
					     struct bytestring delim)
{
	if (whole.len == 0)
		return bytestring_NULL;

	return _splitstr(whole, delim, 0);
}

struct bytestring bytestring_splitstr_next(struct bytestring whole,
					   struct bytestring delim,
					   struct bytestring prev)
{
	if (!prev.ptr)
		return bytestring_NULL;

	/* prev has to be a substring of whole */
	assert(prev.ptr >= whole.ptr);

	if ((prev.ptr + prev.len) == (whole.ptr + whole.len))
		return bytestring_NULL;

	return _splitstr(whole, delim,
			 (prev.ptr - whole.ptr) + prev.len + delim.len);
}
