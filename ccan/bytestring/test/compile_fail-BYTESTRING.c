#include "config.h"

#include <stdio.h>

#include <ccan/bytestring/bytestring.h>

int main(int argc, char *argv[])
{
	struct bytestring bs;

	bs = BYTESTRING(
#ifdef FAIL
		argv[0]
#else
		"literal"
#endif
);
	printf("%zd\n", bs.len);
	return 0;
}
