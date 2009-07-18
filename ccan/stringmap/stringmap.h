/*
 * Copyright (c) 2004 Anders Magnusson (ragge@ludd.luth.se).
 * Copyright (c) 2009 Joseph Adams (joeyadams3.14159@gmail.com).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCAN_STRINGMAP_H
#define CCAN_STRINGMAP_H

#include <ccan/block_pool/block_pool.h>
#include <stdint.h>

#define stringmap(theType) struct {struct stringmap t; struct {char *str; size_t len; theType value;} *last;}
	//the 'last' pointer here is used as a hacky typeof() alternative

#define stringmap_new(ctx) {{0,0,(struct block_pool*)(ctx)},0}
#define stringmap_init(sm, ctx) do { \
		(sm).t.root = 0; \
		(sm).t.count = 0; \
		(sm).t.bp = (struct block_pool*)(ctx); \
		(sm).last = 0; \
	} while(0)
#define stringmap_free(sm) do { \
		if ((sm).t.root) \
			block_pool_free((sm).t.bp); \
	} while(0)

#define stringmap_lookup(sm, key) stringmap_le(sm, key, 0)
#define stringmap_enter(sm, key) stringmap_le(sm, key, 1)

/* Variants of lookup and enter that let you specify a length.  Note that byte
   strings may have null characters in them, and it won't affect the
	algorithm.  Many lives were lost to make this possible. */
#define stringmap_lookup_n(sm, key, len) stringmap_le_n(sm, key, len, 0)
#define stringmap_enter_n(sm, key, len) stringmap_le_n(sm, key, len, 1)

#define stringmap_le(sm, key, enterf) stringmap_le_n(sm, key, (size_t)-1, enterf)

//this macro sets sm.last so it can exploit its type
#define stringmap_le_n(sm, key, len, enterf) ((((sm).last) = stringmap_lookup_real(&(sm).t, key, len, enterf, sizeof(*(sm).last))) ? &(sm).last->value : NULL)


struct stringmap_node;

struct stringmap {
	struct stringmap_node *root;
	size_t count;
	struct block_pool *bp;
	//hack: 'bp' holds talloc ctx when 'root' is NULL
};

void *stringmap_lookup_real(struct stringmap *t, const char *key, size_t len, int enterf, size_t T_size);

#endif
