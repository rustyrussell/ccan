/* CC0 (Public domain) - see LICENSE file for details */
#ifndef CCAN_MEMMEM_H
#define CCAN_MEMMEM_H

#include "config.h"

#include <string.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen);
#endif

#endif /* CCAN_MEMMEM_H */
