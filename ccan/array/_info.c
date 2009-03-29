#include <string.h>
#include "config.h"

/**
 * array - A collection of macros for generic dynamic array management.
 *
 * The array module provides generic dynamic array functions via macros.  It
 * removes the tedium of managing realloc'd arrays with pointer, size, and
 * allocated size.  It also fits into structures quite well.
 * 
 * NOTE:  The API is currently unstable.  It will likely change in the near future.
 * 
 * Example:
 * #include <ccan/array/array.h>
 * #include <stdio.h>
 * 
 * int main(void) {
 * 	Array(int) numbers = NewArray();
 * 	char buffer[32];
 * 	int add;
 * 	
 * 	for (;;) {
 * 		AFor(i, numbers, printf("%d ", *i))
 * 		if (numbers.size) puts("");
 * 		
 * 		printf("array> ");
 * 		fgets(buffer, sizeof(buffer), stdin);
 * 		if (*buffer==0 || *buffer=='\n')
 * 			break;
 * 		add = atoi(buffer);
 * 		
 * 		AAppend(numbers, add);
 * 	}
 * 	
 * 	AFree(numbers);
 * 	
 * 	return 0;
 * }
 *
 * Licence: BSD
 */
int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	if (strcmp(argv[1], "depends") == 0)
		/* Nothing. */
		return 0;

	return 1;
}
