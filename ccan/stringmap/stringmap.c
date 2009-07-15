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

/* This is a heavily modified version of the Patricia tree implementation
   in PCC at http://pcc.zentus.com/cgi-bin/cvsweb.cgi/cc/cpp/cpp.c?rev=1.96 */

#include "stringmap.h"

#if 0
#include <assert.h>
#else
#define assert(...) do {} while(0)
#endif

#define BITNO(x)		((x) & ~(LEFT_IS_LEAF|RIGHT_IS_LEAF))
#define LEFT_IS_LEAF		0x80000000
#define RIGHT_IS_LEAF		0x40000000
#define IS_LEFT_LEAF(x)		(((x) & LEFT_IS_LEAF) != 0)
#define IS_RIGHT_LEAF(x)	(((x) & RIGHT_IS_LEAF) != 0)
#define P_BIT(key, bit)		(key[bit >> 3] >> (bit & 7)) & 1
#define CHECKBITS		8

struct T {
	char *str;
};

static void *T_new(struct block_pool *bp, const char *key, size_t T_size) {
	struct T *leaf = block_pool_alloc(bp, T_size);
	memset(leaf, 0, T_size);
	leaf->str = block_pool_strdup(bp, key);
	return leaf;
}

void *stringmap_lookup_real(struct stringmap *t, const char *key, int enterf, const size_t T_size) {
	struct T *sp;
	struct stringmap_node *w, *new, *last;
	int len, cix, bit, fbit, svbit, ix, bitno;
	const char *k, *m, *sm;
	
	if (!t->root) {
		if (!enterf)
			return NULL;
		
		t->bp = block_pool_new(t->bp);
		
		t->root = T_new(t->bp, key, T_size);
		t->count = 1;
		
		return t->root;
	}

	/* Count full string length */
	for (k = key, len = 0; *k; k++, len++)
		;
	
	if (t->count == 1) {
		w = t->root;
		svbit = 0;
	} else {
		w = t->root;
		bitno = len * CHECKBITS;
		for (;;) {
			bit = BITNO(w->bitno);
			fbit = bit > bitno ? 0 : P_BIT(key, bit);
			svbit = fbit ? IS_RIGHT_LEAF(w->bitno) :
			    IS_LEFT_LEAF(w->bitno);
			w = w->lr[fbit];
			if (svbit)
				break;
		}
	}

	sp = (struct T *)w;

	sm = m = sp->str;
	k = key;

	/* Check for correct string and return */
	for (cix = 0; *m && *k && *m == *k; m++, k++, cix += CHECKBITS)
		;
	if (*m == 0 && *k == 0) {
		//if (!enterf && sp->value == NULL)
		//	return NULL;
		return sp;
	}

	if (!enterf)
		return NULL; /* no string found and do not enter */

	ix = *m ^ *k;
	while ((ix & 1) == 0)
		ix >>= 1, cix++;

	/* Create new node */
	new = block_pool_alloc(t->bp, sizeof *new);
	bit = P_BIT(key, cix);
	new->bitno = cix | (bit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	new->lr[bit] = T_new(t->bp, key, T_size);

	if (t->count++ == 1) {
		new->lr[!bit] = t->root;
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
		t->root = new;
		return (struct T *)new->lr[bit];
	}

	w = t->root;
	last = NULL;
	for (;;) {
		fbit = w->bitno;
		bitno = BITNO(w->bitno);
		assert(bitno != cix);
		if (bitno > cix)
			break;
		svbit = P_BIT(key, bitno);
		last = w;
		w = w->lr[svbit];
		if (fbit & (svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF))
			break;
	}

	new->lr[!bit] = w;
	if (last == NULL) {
		t->root = new;
	} else {
		last->lr[svbit] = new;
		last->bitno &= ~(svbit ? RIGHT_IS_LEAF : LEFT_IS_LEAF);
	}
	if (bitno < cix)
		new->bitno |= (bit ? LEFT_IS_LEAF : RIGHT_IS_LEAF);
	return (struct T *)new->lr[bit];
}
