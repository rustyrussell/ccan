#include "config.h"

#include <stdio.h>

#include <ccan/bytestring/bytestring.h>

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
