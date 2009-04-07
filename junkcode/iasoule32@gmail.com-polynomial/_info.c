#include <string.h>
#include <stdio.h>
#include "config.h"

/**
 * polynomial_adt - A polynomial module with ability to add,sub,mul derivate/integrate, compose ... polynomials 
 *
 * ..expansion in progress ...
 *
 * Example:
 *	FULLY-COMPILABLE-INDENTED-TRIVIAL-BUT-USEFUL-EXAMPLE-HERE
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0) {
		/* Nothing. */
		return 0;
	}

	if (strcmp(argv[1], "libs") == 0) {
		printf("m\n");
		return 0;
	}

	return 1;
}
