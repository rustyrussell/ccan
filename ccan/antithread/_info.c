#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * antithread - Accelerated Native Technology Implementation of "threads"
 *
 * Threads suck.  Antithreads try not to.  FIXME.
 *
 * Licence: LGPL (2 or any later version)
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		printf("ccan/talloc\n");
		printf("ccan/alloc\n");
		printf("ccan/noerr\n");
		printf("ccan/read_write_all\n"); /* For tests */
		return 0;
	}

	return 1;
}
