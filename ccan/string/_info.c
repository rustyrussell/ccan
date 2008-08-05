#include <stdio.h>
#include <string.h>
#include "config.h"

/**
 * string - string helper routines
 *
 * This is a grab bag of modules for string operations, designed to enhance
 * the standard string.h.
 *
 * Example:
 *	#include "string/string.h"
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
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		printf("ccan/talloc\n");
		printf("ccan/noerr\n");
		return 0;
	}

	return 1;
}
