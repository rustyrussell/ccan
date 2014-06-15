/* CC0 (Public domain) - see LICENSE file for details */

#include "config.h"

#include <string.h>
#include <ccan/memmem/memmem.h>

#if !HAVE_MEMMEM
void *memmem(const void *haystack, size_t haystacklen,
	     const void *needle, size_t needlelen)
{
	const char *p;

	if (needlelen > haystacklen)
		return NULL;

	p = haystack;

	for (p = haystack;
	     (p + needlelen) <= ((const char *)haystack + haystacklen);
	     p++)
		if (memcmp(p, needle, needlelen) == 0)
			return (void *)p;

	return NULL;
}
#endif
