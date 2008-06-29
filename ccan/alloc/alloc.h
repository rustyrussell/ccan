#ifndef ALLOC_H
#define ALLOC_H
#include <stdio.h>
#include <stdbool.h>

void alloc_init(void *pool, unsigned long poolsize);
void *alloc_get(void *pool, unsigned long poolsize,
		unsigned long size, unsigned long align);
void alloc_free(void *pool, unsigned long poolsize, void *free);
unsigned long alloc_size(void *pool, unsigned long poolsize, void *p);
bool alloc_check(void *pool, unsigned long poolsize);

void alloc_visualize(FILE *out, void *pool, unsigned long poolsize);
#endif /* ALLOC_H */
