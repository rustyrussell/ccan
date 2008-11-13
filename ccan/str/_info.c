#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * str - string helper routines
 *
 * This is a grab bag of functions for string operations, designed to enhance
 * the standard string.h.
 *
 * Example:
 *	#include <stdio.h>
 *	#include <ccan/str/str.h>
 *
 *	int main(int argc, char *argv[])
 *	{
 *		if (argv[1] && streq(argv[1], "--verbose"))
 *			printf("verbose set\n");
 *		if (argv[1] && strstarts(argv[1], "--"))
 *			printf("Some option set\n");
 *		if (argv[1] && strends(argv[1], "cow-powers"))
 *			printf("Magic option set\n");
 *		return 0;
 *	}
 *
 * Licence: LGPL (2 or any later version)
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		return 0;
	}

	return 1;
}
