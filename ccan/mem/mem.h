/* CC0 (Public domain) - see LICENSE file for details */
#ifndef CCAN_MEM_H
#define CCAN_MEM_H

#include "config.h"

#include <string.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen);
#endif

#if !HAVE_MEMRCHR
void *memrchr(const void *s, int c, size_t n);
#endif

#endif /* CCAN_MEM_H */
