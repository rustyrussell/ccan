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

#include "block_pool.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//must be a power of 2
#define BLOCK_SIZE 4096

struct block {
	size_t remaining;
	size_t size;
	char *data;
};

struct block_pool {
	size_t count;
	size_t alloc; //2^n - 1, where n is an integer > 1
	struct block *block;
	
	//blocks are arranged in a max-heap by the .remaining field
	// (except the root block does not percolate down until it is filled)
};

static int destructor(struct block_pool *bp) {
	struct block *block = bp->block;
	size_t d = bp->count;
	
	for (;d--;block++)
		free(block->data);
	free(bp->block);
	
	return 0;
}

struct block_pool *block_pool_new(void *ctx) {
	struct block_pool *bp = talloc(ctx, struct block_pool);
	talloc_set_destructor(bp, destructor);
	
	bp->count = 0;
	bp->alloc = 7;
	bp->block = malloc(bp->alloc * sizeof(struct block));
	
	return bp;
}

static void *new_block(struct block *b, size_t needed) {
	b->size = (needed+(BLOCK_SIZE-1)) & ~(BLOCK_SIZE-1);
	b->remaining = b->size - needed;
	b->data = malloc(b->size);
	return b->data;
}

//for the first block, keep the memory usage low in case it's the only block.
static void *new_block_tiny(struct block *b, size_t needed) {
	if (needed < 256)
		b->size = 256;
	else
		b->size = (needed+(BLOCK_SIZE-1)) & ~(BLOCK_SIZE-1);
	b->remaining = b->size - needed;
	b->data = malloc(b->size);
	return b->data;
}

static void *try_block(struct block *b, size_t size, size_t align) {
	size_t offset = b->size - b->remaining;
	offset = (offset+align) & ~align;
	
	if (b->size-offset >= size) {
		//good, we can use this block
		void *ret = b->data + offset;
		b->remaining = b->size-offset-size;
		
		return ret;
	}
	
	return NULL;
}

#define L(node) (node+node+1)
#define R(node) (node+node+2)
#define P(node) ((node-1)>>1)

#define V(node) (bp->block[node].remaining)

static void percolate_down(struct block_pool *bp, size_t node) {
	size_t child = L(node);
	struct block tmp;
	
	//get the maximum child
	if (child >= bp->count)
		return;
	if (child+1 < bp->count && V(child+1) > V(child))
		child++;
	
	if (V(child) <= V(node))
		return;
	
	tmp = bp->block[node];
	bp->block[node] = bp->block[child];
	bp->block[child] = tmp;
	
	percolate_down(bp, child);
}

//note:  percolates up to either 1 or 2 as a root
static void percolate_up(struct block_pool *bp, size_t node) {
	size_t parent = P(node);
	struct block tmp;
	
	if (node<3 || V(parent) >= V(node))
		return;
	
	tmp = bp->block[node];
	bp->block[node] = bp->block[parent];
	bp->block[parent] = tmp;
	
	percolate_up(bp, parent);
}

void *block_pool_alloc_align(struct block_pool *bp, size_t size, size_t align) {
	void *ret;
	
	if (align)
		align--;
	
	//if there aren't any blocks, make a new one
	if (!bp->count) {
		bp->count = 1;
		return new_block_tiny(bp->block, size);
	}
	
	//try the root block
	ret = try_block(bp->block, size, align);
	if (ret)
		return ret;
	
	//root block is filled, percolate down and try the biggest one
	percolate_down(bp, 0);
	ret = try_block(bp->block, size, align);
	if (ret)
		return ret;
	
	//the biggest wasn't big enough; we need a new block
	if (bp->count >= bp->alloc) {
		//make room for another block
		bp->alloc += bp->alloc;
		bp->alloc++;
		bp->block = realloc(bp->block, bp->alloc * sizeof(struct block));
	}
	ret = new_block(bp->block+(bp->count++), size);
	
	//fix the heap after adding the new block
	percolate_up(bp, bp->count-1);
	
	return ret;
}

#undef L
#undef R
#undef P
#undef V

char *block_pool_strdup(struct block_pool *bp, const char *str) {
	size_t size = strlen(str)+1;
	char *ret = block_pool_alloc_align(bp, size, 1);
	
	memcpy(ret, str, size);
	return ret;
}
