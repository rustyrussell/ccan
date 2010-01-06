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

#include <ccan/stringmap/stringmap.h>

//#define CONSISTENCY_CHECK

#if 0
#include <assert.h>
#else
#define assert(...) do {} while(0)
#endif

#define PEEK_BIT(key, bit)		((key[bit >> 3] >> (~bit & 7)) & 1)

struct stringmap_node {
	uint32_t left_is_leaf:1, right_is_leaf:1, bitno:30;
	struct stringmap_node *lr[2];
};

struct T {
	char *str;
	size_t len;
};

static inline struct T *leaf(struct stringmap_node *n, int lr) {
	assert(lr ? n->right_is_leaf : n->left_is_leaf);
	return (struct T*)n->lr[lr];
}

/* Normal nodes diverge because there was a 0 or 1 difference. If left_ends(n),
   then the node diverges because one string ends and the rest don't. */
static inline int left_ends(struct stringmap_node *n) {
	return (n->left_is_leaf && (leaf(n,0)->len << 3)==n->bitno);
}

static void *T_new(struct block_pool *bp, const char *key, size_t len, size_t T_size) {
	struct T *leaf = block_pool_alloc(bp, T_size);
	memset(leaf, 0, T_size);
	
	leaf->str = block_pool_alloc_align(bp, len+1, 1);
	memcpy(leaf->str, key, len);
	leaf->str[len] = 0;
	leaf->len = len;
	
	return leaf;
}

//used for diagnostics
static int consistency_check(struct stringmap *t);
static void emit_dot(struct stringmap *t);
static void emit_subtree(struct stringmap_node *n, int is_leaf);

void *stringmap_lookup_real(struct stringmap *t, const char *key, size_t len, int enterf, size_t T_size) {
	struct T *sp;
	struct stringmap_node *w, *new, *last;
	uint32_t cix, bit, svbit, ix, bitno, end_bit;
	const char *k, *m;
	
	(void) consistency_check;
	(void) emit_dot;
	#ifdef STRINGMAP_EMIT_DOT
	emit_dot(t);
	#endif
	#ifdef CONSISTENCY_CHECK
	consistency_check(t);
	#endif
	
	/* If key length wasn't supplied, calculate it. */
	if (len == (size_t)-1)
		len = strlen(key);
	end_bit = len << 3;
	
	/* If tree is empty, create the first node. */
	if (!t->root) {
		if (!enterf)
			return NULL;
		
		t->bp = block_pool_new(t->bp);
		
		t->root = T_new(t->bp, key, len, T_size);
		t->count = 1;
		
		return t->root;
	}
	
	/* Follow the tree down to what might be the target key. */
	if (t->count == 1) {
		w = t->root;
		svbit = 0;
	} else {
		w = t->root;
		for (;;) {
			if (!left_ends(w)) //0 or 1
				bit = w->bitno < end_bit ? PEEK_BIT(key, w->bitno) : 0;
			else //ends or doesn't end
				bit = (w->bitno != end_bit);
			svbit = bit ? w->right_is_leaf : w->left_is_leaf;
			w = w->lr[bit];
			if (svbit)
				break;
		}
	}
	
	/* See if the strings match.  If not, set cix to the first bit offset
	   where there's a difference, and bit to the side on which to put
		this leaf. */
	sp = (struct T *)w;
	m = sp->str;
	k = key;
	for (cix = 0; ; m++, k++, cix++) {
		if (cix>=sp->len || cix>=len) { //we reached the end of one or both strings
			if (cix==sp->len && cix==len) { //strings match
				//if (!enterf && sp->value == NULL)
				//	return NULL;
				return sp;
			}
			cix <<= 3;
			
			//put the shorter key to the left
			bit = len > sp->len;
			
			break;
		}
		if (*m != *k) { //the strings have a differing character
			cix <<= 3;
			
			//advance cix to the first differing bit
			ix = *m ^ *k;
			while ((ix & 128) == 0)
				ix <<= 1, cix++;
			
			//choose left/right based on the differing bit
			bit = PEEK_BIT(key, cix);
			
			break;
		}
	}
	
	if (!enterf)
		return NULL; /* no string found and do not enter */

	/* Create new node */
	new = block_pool_alloc(t->bp, sizeof *new);
	
	new->right_is_leaf = bit;
	new->left_is_leaf = !bit;
	new->bitno = cix;
	
	new->lr[bit] = T_new(t->bp, key, len, T_size);

	if (t->count++ == 1) {
		new->lr[!bit] = t->root;
		new->right_is_leaf = 1;
		new->left_is_leaf = 1;
		t->root = new;
		return (struct T *)new->lr[bit];
	}

	w = t->root;
	last = NULL;
	for (;;) {
		bitno = w->bitno;
		if (bitno > cix)
			break;
		
		if (!left_ends(w)) { //0 or 1
			if (bitno == cix)
				break;
			svbit = PEEK_BIT(key, bitno);
			
		} else { //ends or doesn't end
			//because left is an end, we cannot split it, so we must turn right
			svbit = 1;
		}
		
		last = w;
		w = w->lr[svbit];
		if (svbit ? last->right_is_leaf : last->left_is_leaf) {
			//w is a leaf, so mark it accordingly in its parent structure
			if (!bit)
				new->right_is_leaf = 1;
			else
				new->left_is_leaf = 1;
			
			break;
		}
	}

	new->lr[!bit] = w;
	if (last == NULL) {
		t->root = new;
	} else {
		last->lr[svbit] = new;
		if (svbit)
			last->right_is_leaf = 0;
		else
			last->left_is_leaf = 0;
	}
	
	return (struct T *)new->lr[bit];
}

static int consistency_check_subtree(struct stringmap_node *n) {
	uint32_t bitno = n->bitno;
	int success = 1;
	
	//make sure bitnos ascend (must ascend unless left ends)
	if (!n->left_is_leaf && bitno >= n->lr[0]->bitno) {
		printf("Left leaf has bitno >= than parent\n");
		success = 0;
	}
	if (!n->right_is_leaf && bitno >= n->lr[1]->bitno) {
		if (left_ends(n) && bitno == n->lr[1]->bitno) {
			//fine, there's a shelf here
		} else {
			printf("Right leaf has bitno >= than parent\n");
			success = 0;
		}
	}
	
	//make sure eponymous bits are set properly
	if (n->left_is_leaf) {
		struct T *lf = leaf(n, 0);
		size_t len = lf->len << 3;
		if (len == n->bitno) {
			//this is a shelf
		} else if (len <= n->bitno) {
			printf("Left leaf is too short\n");
			success = 0;
		} else if (PEEK_BIT(lf->str, n->bitno) == 1) {
			printf("Left leaf has incorrect bit\n");
			success = 0;
		}
	}
	if (n->right_is_leaf) {
		struct T *lf = leaf(n, 1);
		size_t len = lf->len << 3;
		if (len <= n->bitno) {
			printf("Right leaf is too short\n");
			success = 0;
		} else if (PEEK_BIT(lf->str, n->bitno) == 0 && !left_ends(n)) {
			printf("Right leaf has incorrect bit\n");
			success = 0;
		}
	}
	
	if (!success) {
		//emit_subtree(n, 0);
		abort();
	}
	
	//recursively check
	return (!n->left_is_leaf ? consistency_check_subtree(n->lr[0]) : 1) &&
	      (!n->right_is_leaf ? consistency_check_subtree(n->lr[1]) : 1);
}

static int consistency_check(struct stringmap *t) {
	if (t->count < 2)
		return 1;
	return consistency_check_subtree(t->root);
}

//The following can be used to create Graphviz "dot" files to visualize the tree

static void leaf_to_dot(void *lp, FILE *f) {
	struct T *leaf = lp;
	size_t bit_count = leaf->len << 3;
	size_t i;
	
	fputs("\"", f);
	#if 1
	for (i=0; i<bit_count; i++) {
		putc(PEEK_BIT(leaf->str, i) ? '1' : '0', f);
		if (((i+1) & 7) == 0)
			fputs("\\n", f); //add newlines between bytes
	}
	putc(' ', f);
	#endif
	fprintf(f, "(%s)\"\n", leaf->str);
}

static void node_to_dot(struct stringmap_node *n, FILE *f, size_t level) {
	//don't draw ridiculously huge trees
	if (level > 4)
		return;
	
	fprintf(f, "%zu [label=\"[%zu] %u\"]\n", (size_t)n, level, n->bitno);
	
	if (n->left_is_leaf) {
		fprintf(f, "%zu -> ", (size_t)n);
		leaf_to_dot(n->lr[0], f);
	} else {
		fprintf(f, "%zu -> %zu \n", (size_t)n, (size_t)n->lr[0]);
		node_to_dot(n->lr[0], f, level+1);
	}
	
	if (n->right_is_leaf) {
		fprintf(f, "%zu -> ", (size_t)n);
		leaf_to_dot(n->lr[1], f);
	} else {
		fprintf(f, "%zu -> %zu \n", (size_t)n, (size_t)n->lr[1]);
		node_to_dot(n->lr[1], f, level+1);
	}
}

static void stringmap_subtree_to_dot(struct stringmap_node *n, int is_leaf, const char *filename_out) {
	FILE *f = fopen(filename_out, "w");
	
	fputs("digraph G {\n", f);
	
	if (is_leaf)
		leaf_to_dot(n, f);
	else
		node_to_dot(n, f, 0);
	
	fputs("}\n", f);
	fclose(f);
}

static size_t dot_file_number = 0;

static void emit_subtree(struct stringmap_node *n, int is_leaf) {
	char buf[64];
	sprintf(buf, "dot/%04zu.dot", dot_file_number++);
	stringmap_subtree_to_dot(n, is_leaf, buf);
}

static void emit_dot(struct stringmap *t) {
	if (t->count)
		emit_subtree(t->root, t->count==1);
}
