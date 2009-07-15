/*
        Copyright (c) 2009  Joseph A. Adams
        All rights reserved.
        
        Redistribution and use in source and binary forms, with or without
        modification, are permitted provided that the following conditions
        are met:
        1. Redistributions of source code must retain the above copyright
           notice, this list of conditions and the following disclaimer.
        2. Redistributions in binary form must reproduce the above copyright
           notice, this list of conditions and the following disclaimer in the
           documentation and/or other materials provided with the distribution.
        3. The name of the author may not be used to endorse or promote products
           derived from this software without specific prior written permission.
        
        THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
        IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
        OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
        IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
        INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
        NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
        DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
        THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
        (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
        THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
