#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#define _XOPEN_SOURCE 700
#include <string.h>
#include "xstring.h"

xstring *xstrNew(const size_t size)
{
	char *str;
	xstring *x;

	if (size < 1) {
		errno = EINVAL;
		return NULL;
	}

	str = malloc(size);
	if (!str)
		return NULL;

	x = malloc(sizeof(struct xstring));
	if (!x) {
		free(str);
		return NULL;
	}

	xstrInit(x, str, size, 0);
	return x;
}

int xstrInit(xstring *x, char *str, const size_t size, int keep)
{
	assert(x && str && size > 0);
	memset(x, 0, sizeof(*x));

	x->str = str;
	x->cap = size;

	if (keep) {
		x->len = strnlen(str, x->cap);
		if (x->cap == x->len) {
			x->truncated = -1;
			x->len--;
		}
	}

	*(x->str + x->len) = '\0';

	return x->truncated;
}

int xstrAddChar(xstring *x, const char c)
{
	assert(x);

	if (x->truncated || c == '\0')
		return x->truncated;

	if (x->len + 1 < x->cap) {
		char *p = x->str + x->len;
		*p++ = c;
		*p = '\0';
		x->len++;
	}
	else
		x->truncated = -1;

	return x->truncated;
}

int xstrAdd(xstring *x, const char *src)
{
	char *p, *q, *s;

	assert(x);

	if (x->truncated || !src || *src == '\0')
		return x->truncated;

	for (s = (char *)src,
	     p = x->str + x->len,
	     q = x->str + x->cap - 1;
	     *s != '\0' && p < q; p++, s++) {
		*p = *s;
	}

	*p = '\0';
	x->len = (size_t) (p - x->str);

	if (*s != '\0')
		x->truncated = -1;

	return x->truncated;
}

int xstrAddSub(xstring *x, const char *src, size_t len)
{
	assert(x);

	if (x->truncated || !src || len == 0)
		return x->truncated;

	if (x->len + len + 1 > x->cap)
		return x->truncated = -1;

	memcpy(x->str + x->len, src, len);

	x->len += len;
	*(x->str + x->len) = '\0';

	return x->truncated;
}

int xstrCat(xstring *x, ...)
{
	va_list ap;
	char *s;

	assert(x);
	va_start(ap, x);
	while ((s = va_arg(ap, char *)) != NULL) {
		if (xstrAdd(x, s) == -1)
			break;
	}
	va_end(ap);
	return x->truncated;
}

int xstrJoin(xstring *x, const char *sep, ...)
{
	va_list ap;
	char *s;
	int i;

	assert(x && sep);
	va_start(ap, sep);
	for (i = 0; (s = va_arg(ap, char *)) != NULL; i++) {
		if (i && xstrAdd(x, sep) == -1)
			break;
		if (xstrAdd(x, s) == -1)
			break;
	}
	va_end(ap);
	return x->truncated;
}

int xstrAddSubs(xstring *x, ...)
{
	va_list ap;
	char *s;

	assert(x);
	va_start(ap, x);
	while ((s = va_arg(ap, char *)) != NULL) {
		size_t n = va_arg(ap, size_t);
		if (xstrAddSub(x, s, n) == -1)
			break;
	}
	va_end(ap);
	return x->truncated;
}

int xstrEq3(xstring *x, xstring *y)
{
	assert(x && y);

	if (x->truncated || y->truncated)
		return unknown;

	return x->len == y->len && xstrContains3(x, y, 1);
}

/* Does the first string contain the second */
int xstrContains3(xstring *x, xstring *y, int where)
{
	int b;

	assert(x && y && where >= -1 && where <= 1);

	if (y->truncated)
		return unknown;

	if (x->len < y->len)
		return 0;

	switch (where) {
	case -1:
		b = strncmp(x->str + x->len - y->len, y->str, y->len) == 0;
		break;
	case 0:
		b = strstr(x->str, y->str) != NULL;
		break;
	case 1:
		b = strncmp(x->str, y->str, y->len) == 0;
		break;
	}

	return b ? 1 : x->truncated ? unknown : 0;
}

void xstrFree(xstring *x)
{
	assert(x);
	free(x->str);
	free(x);
}
