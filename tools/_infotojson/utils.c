#include "utils.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include "utils.h"

void * palloc(int size)
{
	void *p;
	p = malloc(size);
	if(p == NULL) {
		printf("Error Malloc does not allocate\n");
		exit(1);
	}
	return p;
}

char *aprintf(const char *fmt, ...)
{
	char *ret;
	va_list arglist;

	va_start(arglist, fmt);
	vasprintf(&ret, fmt, arglist);
	va_end(arglist);
	return ret;
}

int strreplace(char * str, char src, char dest)
{
	int i;
	for(i = 0; str[i]; i++)
		if(str[i] == src)
			str[i] = dest;
}

void *_realloc_array(void *ptr, size_t size, size_t num)
{
        if (num >= SIZE_MAX/size)
                return NULL;
        return realloc_nofail(ptr, size * num);
}

void *realloc_nofail(void *ptr, size_t size)
{
        ptr = realloc(ptr, size);
	if (ptr)
		return ptr;
	printf("realloc of %zu failed", size);
}
