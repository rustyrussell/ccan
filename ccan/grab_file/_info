#include "config.h"
#include <stdio.h>
#include <string.h>

/**
 * grab_file - file helper routines
 *
 * This contains simple functions for getting the contents of a file.
 *
 * Example:
 *	#include <err.h>
 *	#include <stdio.h>
 *	#include <string.h>
 *	#include <ccan/grab_file/grab_file.h>
 *	#include <ccan/talloc/talloc.h>	// For talloc_free()
 *
 *	int main(int argc, char *argv[])
 *	{
 *		size_t len;
 *		char *file;
 *
 *		file = grab_file(NULL, argv[1], &len);
 *		if (!file)
 *			err(1, "Could not read file %s", argv[1]);
 *		if (strlen(file) != len)
 *			printf("File contains NUL characters\n");
 *		else if (len == 0)
 *			printf("File contains nothing\n");
 *		else if (strchr(file, '\n'))
 *			printf("File contains multiple lines\n");
 *		else
 *			printf("File contains one line\n");
 *		talloc_free(file);
 *
 *		return 0;
 *	}
 *
 * License: LGPL (v2.1 or any later version)
 * Author: Rusty Russell <rusty@rustcorp.com.au>
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
