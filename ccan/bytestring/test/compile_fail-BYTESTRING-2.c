#include "config.h"

#include <stdio.h>

#include <ccan/bytestring/bytestring.h>

/*
 * BYTESTRING() can only be used safely on a literal string (or,
 * strictly, something whose size can be determined with ARRAY_SIZE().
 * This checks that it correctly fails to compile if used on a
 * non-array pointer.
 */
int main(int argc, char *argv[])
{
	struct bytestring bs;
	const char *x = "abcde";

#ifdef FAIL
	bs = BYTESTRING(x);
#else
	bs.len = 0;
#endif
	printf("%zd %s\n", bs.len, x);
	return 0;
}
