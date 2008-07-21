#include <stdarg.h>
#include <stdbool.h>
#include <string.h>


#define new_array(type, num) realloc_array((type *)0, (num))
#define realloc_array(ptr, num) ((__typeof__(ptr))_realloc_array((ptr), sizeof((*ptr)), (num)))

void *realloc_nofail(void *ptr, size_t size);

void *_realloc_array(void *ptr, size_t size, size_t num);

void * palloc(int size);

char *aprintf(const char *fmt, ...);

int strreplace(char * str, char src, char dest);
