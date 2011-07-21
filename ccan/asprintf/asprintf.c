/* Licensed under BSD-MIT - see LICENSE file for details */
#include <ccan/asprintf/asprintf.h>
#include <stdarg.h>
#include <stdio.h>

char *PRINTF_FMT(1, 2) afmt(const char *fmt, ...)
{
	va_list ap;
	char *ptr;

	va_start(ap, fmt);
	/* The BSD version apparently sets ptr to NULL on fail.  GNU loses. */
	if (vasprintf(&ptr, fmt, ap) < 0)
		ptr = NULL;
	va_end(ap);
	return ptr;
}

#if !HAVE_ASPRINTF
#include <stdarg.h>
#include <stdlib.h>

int vasprintf(char **strp, const char *fmt, va_list ap)
{
	int len;
	va_list ap_copy;

	/* We need to make a copy of ap, since it's a use-once. */
	va_copy(ap_copy, ap);
	len = vsnprintf(NULL, 0, fmt, ap_copy);
	va_end(ap_copy);

	/* Until version 2.0.6 glibc would return -1 on truncated output.
	 * OTOH, they had asprintf. */
	if (len < 0)
		return -1;

	*strp = malloc(len+1);
	if (!*strp)
		return -1;

	return vsprintf(*strp, fmt, ap);
}

int asprintf(char **strp, const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vasprintf(strp, fmt, ap);
	va_end(ap);

	return len;
}
#endif /* !HAVE_ASPRINTF */
