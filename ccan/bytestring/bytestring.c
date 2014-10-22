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
