#include <string.h>

/**
 * crc - routines for crc of bytes
 *
 * Cyclic Redundancy Check routines.  These are reasonably fast
 * checksum routines, but not suitable for cryptographic use.
 *
 * They are useful for simple error detection, eg. a 32-bit CRC will
 * detect a single error burst of up to 32 bits.
 *
 * Example:
 *	#include <ccan/crc.h>
 *	#include <stdio.h>
 *	#include <stdlib.h>
 *
 *	int main(int argc, char *argv[])
 *	{
 *		if (argc != 2) {
 *			fprintf(stderr, "Usage: %s <string>\n"
 *				"Prints 32 bit CRC of the string\n", argv[0]);
 *			exit(1);
 *		}
 *		printf("0x%08x\n", crc32c(argv[1], strlen(argv[1])));
 *		exit(0);
 *	}
 *
 * Licence: GPL (v2 or any later version)
 * Author: Gary S. Brown, Clay Haapala
 * Maintainer: Rusty Russell <rusty@rustcorp.com.au>
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
