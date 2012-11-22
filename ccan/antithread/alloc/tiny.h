/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_TINY_H
#define CCAN_TINY_H
#include <stdbool.h>
#include <stdio.h>

void tiny_alloc_init(void *pool, unsigned long poolsize);
void *tiny_alloc_get(void *pool, unsigned long poolsize,
		     unsigned long size, unsigned long align);
void tiny_alloc_free(void *pool, unsigned long poolsize, void *free);
unsigned long tiny_alloc_size(void *pool, unsigned long poolsize, void *p);
bool tiny_alloc_check(void *pool, unsigned long poolsize);
void tiny_alloc_visualize(FILE *out, void *pool, unsigned long poolsize);

#endif /* CCAN_TINY_H */
