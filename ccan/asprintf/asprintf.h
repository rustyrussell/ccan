/* Licensed under BSD-MIT - see LICENSE file for details */
#ifndef CCAN_ASPRINTF_H
#define CCAN_ASPRINTF_H
#include "config.h"
#include <ccan/compiler/compiler.h>

/**
 * afmt - allocate and populate a string with the given format.
 * @fmt: printf-style format.
 *
 * This is a simplified asprintf interface.  Returns NULL on error.
 */
char *PRINTF_FMT(1, 2) afmt(const char *fmt, ...);

#if HAVE_ASPRINTF
#include <stdio.h>
#else
#include <stdarg.h>
/**
 * asprintf - printf to a dynamically-allocated string.
 * @strp: pointer to the string to allocate.
 * @fmt: printf-style format.
 *
 * Returns -1 (and leaves @strp undefined) on an error.  Otherwise returns
 * number of bytes printed into @strp.
 *
 * Example:
 *	static char *greeting(const char *name)
 *	{
 *		char *str;
 *		int len = asprintf(&str, "Hello %s", name);
 *		if (len < 0)
 *			return NULL;
 *		return str;
 *	}
 */
int PRINTF_FMT(2, 3) asprintf(char **strp, const char *fmt, ...);

/**
 * vasprintf - vprintf to a dynamically-allocated string.
 * @strp: pointer to the string to allocate.
 * @fmt: printf-style format.
 *
 * Returns -1 (and leaves @strp undefined) on an error.  Otherwise returns
 * number of bytes printed into @strp.
 */
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#endif /* CCAN_ASPRINTF_H */
