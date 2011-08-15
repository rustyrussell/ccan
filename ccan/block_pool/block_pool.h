/*
 * Copyright (C) 2009 Joseph Adams <joeyadams3.14159@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef CCAN_BLOCK_POOL
#define CCAN_BLOCK_POOL

#include <ccan/talloc/talloc.h>
#include <string.h>

struct block_pool;

/* Construct a new block pool.
   ctx is a talloc context (or NULL if you don't know what talloc is ;) ) */
struct block_pool *block_pool_new(void *ctx);

/* Same as block_pool_alloc, but allows you to manually specify alignment.
   For instance, strings need not be aligned, so set align=1 for them.
   align must be a power of two. */
void *block_pool_alloc_align(struct block_pool *bp, size_t size, size_t align);

/* Allocate a block of a given size.  The returned pointer will remain valid
   for the life of the block_pool.  The block cannot be resized or
   freed individually. */
static inline void *block_pool_alloc(struct block_pool *bp, size_t size) {
	size_t align = size & -size; //greatest power of two by which size is divisible
	if (align > 16)
		align = 16;
	return block_pool_alloc_align(bp, size, align);
}

static inline void block_pool_free(struct block_pool *bp) {
	talloc_free(bp);
}


char *block_pool_strdup(struct block_pool *bp, const char *str);

static inline void *block_pool_memdup(struct block_pool *bp, const void *src, size_t size) {
	void *ret = block_pool_alloc(bp, size);
	memcpy(ret, src, size);
	return ret;
}

#endif
