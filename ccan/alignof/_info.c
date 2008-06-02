#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * alignof - ALIGNOF() macro to determine alignment of a type.
 *
 * Many platforms have requirements that certain types must be aligned
 * to certain address boundaries, such as ints needing to be on 4-byte
 * boundaries.  Attempting to access variables with incorrect
 * alignment may cause performance loss or even program failure (eg. a
 * bus signal).
 *
 * There are times which it's useful to be able to programatically
 * access these requirements, such as for dynamic allocators.
 *
 * Example:
 *	#include <stdio.h>
 *	#include "alignof/alignoff.h"
 *
 *	int main(int argc, char *argv[])
 *	{
 *		char arr[sizeof(int)];
 *
 *		if ((unsigned long)arr % ALIGNOF(int)) {
 *			printf("arr %p CANNOT hold an int\n", arr);
 *			exit(1);
 *		} else {
 *			printf("arr %p CAN hold an int\n", arr);
 *			exit(0);
 *		}
 *	}
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		printf("ccan/build_assert\n");
		return 0;
	}

	return 1;
}
