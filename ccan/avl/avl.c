/*
 * Copyright (C) 2010 Joseph Adams <joeyadams3.14159@gmail.com>
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

#include "avl.h"

#include <assert.h>
#include <stdlib.h>

static AvlNode *mkNode(const void *key, const void *value);
static void freeNode(AvlNode *node);

static AvlNode *lookup(const AVL *avl, AvlNode *node, const void *key);

static bool insert(AVL *avl, AvlNode **p, const void *key, const void *value);
static bool remove(AVL *avl, AvlNode **p, const void *key, AvlNode **ret);
static bool removeExtremum(AvlNode **p, int side, AvlNode **ret);

static int sway(AvlNode **p, int sway);
static void balance(AvlNode **p, int side);

static bool checkBalances(AvlNode *node, int *height);
static bool checkOrder(AVL *avl);
static size_t countNode(AvlNode *node);

/*
 * Utility macros for converting between
 * "balance" values (-1 or 1) and "side" values (0 or 1).
 *
 * bal(0)   == -1
 * bal(1)   == +1
 * side(-1) == 0
 * side(+1) == 1
 */
#define bal(side) ((side) == 0 ? -1 : 1)
#define side(bal) ((bal)  == 1 ?  1 : 0)

static int sign(int cmp)
{
	if (cmp < 0)
		return -1;
	if (cmp == 0)
		return 0;
	return 1;
}

AVL *avl_new(AvlCompare compare)
{
	AVL *avl = malloc(sizeof(*avl));
	
	assert(avl != NULL);
	
	avl->compare = compare;
	avl->root = NULL;
	avl->count = 0;
	return avl;
}

void avl_free(AVL *avl)
{
	freeNode(avl->root);
	free(avl);
}

void *avl_lookup(const AVL *avl, const void *key)
{
	AvlNode *found = lookup(avl, avl->root, key);
	return found ? (void*) found->value : NULL;
}

AvlNode *avl_lookup_node(const AVL *avl, const void *key)
{
	return lookup(avl, avl->root, key);
}

size_t avl_count(const AVL *avl)
{
	return avl->count;
}

bool avl_insert(AVL *avl, const void *key, const void *value)
{
	size_t old_count = avl->count;
	insert(avl, &avl->root, key, value);
	return avl->count != old_count;
}

bool avl_remove(AVL *avl, const void *key)
{
	AvlNode *node = NULL;
	
	remove(avl, &avl->root, key, &node);
	
	if (node == NULL) {
		return false;
	} else {
		free(node);
		return true;
	}
}

static AvlNode *mkNode(const void *key, const void *value)
{
	AvlNode *node = malloc(sizeof(*node));
	
	assert(node != NULL);
	
	node->key = key;
	node->value = value;
	node->lr[0] = NULL;
	node->lr[1] = NULL;
	node->balance = 0;
	return node;
}

static void freeNode(AvlNode *node)
{
	if (node) {
		freeNode(node->lr[0]);
		freeNode(node->lr[1]);
		free(node);
	}
}

static AvlNode *lookup(const AVL *avl, AvlNode *node, const void *key)
{
	int cmp;
	
	if (node == NULL)
		return NULL;
	
	cmp = avl->compare(key, node->key);
	
	if (cmp < 0)
		return lookup(avl, node->lr[0], key);
	if (cmp > 0)
		return lookup(avl, node->lr[1], key);
	return node;
}

/*
 * Insert a key/value into a subtree, rebalancing if necessary.
 *
 * Return true if the subtree's height increased.
 */
static bool insert(AVL *avl, AvlNode **p, const void *key, const void *value)
{
	if (*p == NULL) {
		*p = mkNode(key, value);
		avl->count++;
		return true;
	} else {
		AvlNode *node = *p;
		int      cmp  = sign(avl->compare(key, node->key));
		
		if (cmp == 0) {
			node->key = key;
			node->value = value;
			return false;
		}
		
		if (!insert(avl, &node->lr[side(cmp)], key, value))
			return false;
		
		/* If tree's balance became -1 or 1, it means the tree's height grew due to insertion. */
		return sway(p, cmp) != 0;
	}
}

/*
 * Remove the node matching the given key.
 * If present, return the removed node through *ret .
 * The returned node's lr and balance are meaningless.
 *
 * Return true if the subtree's height decreased.
 */
static bool remove(AVL *avl, AvlNode **p, const void *key, AvlNode **ret)
{
	if (*p == NULL) {
		return false;
	} else {
		AvlNode *node = *p;
		int      cmp  = sign(avl->compare(key, node->key));
		
		if (cmp == 0) {
			*ret = node;
			avl->count--;
			
			if (node->lr[0] != NULL && node->lr[1] != NULL) {
				AvlNode *replacement;
				int      side;
				bool     shrunk;
				
				/* Pick a subtree to pull the replacement from such that
				 * this node doesn't have to be rebalanced. */
				side = node->balance <= 0 ? 0 : 1;
				
				shrunk = removeExtremum(&node->lr[side], 1 - side, &replacement);
				
				replacement->lr[0]   = node->lr[0];
				replacement->lr[1]   = node->lr[1];
				replacement->balance = node->balance;
				*p = replacement;
				
				if (!shrunk)
					return false;
				
				replacement->balance -= bal(side);
				
				/* If tree's balance became 0, it means the tree's height shrank due to removal. */
				return replacement->balance == 0;
			}
			
			if (node->lr[0] != NULL)
				*p = node->lr[0];
			else
				*p = node->lr[1];
			
			return true;
			
		} else {
			if (!remove(avl, &node->lr[side(cmp)], key, ret))
				return false;
			
			/* If tree's balance became 0, it means the tree's height shrank due to removal. */
			return sway(p, -cmp) == 0;
		}
	}
}

/*
 * Remove either the left-most (if side == 0) or right-most (if side == 1)
 * node in a subtree, returning the removed node through *ret .
 * The returned node's lr and balance are meaningless.
 *
 * The subtree must not be empty (i.e. *p must not be NULL).
 *
 * Return true if the subtree's height decreased.
 */
static bool removeExtremum(AvlNode **p, int side, AvlNode **ret)
{
	AvlNode *node = *p;
	
	if (node->lr[side] == NULL) {
		*ret = node;
		*p = node->lr[1 - side];
		return true;
	}
	
	if (!removeExtremum(&node->lr[side], side, ret))
		return false;
	
	/* If tree's balance became 0, it means the tree's height shrank due to removal. */
	return sway(p, -bal(side)) == 0;
}

/*
 * Rebalance a node if necessary.  Think of this function
 * as a higher-level interface to balance().
 *
 * sway must be either -1 or 1, and indicates what was added to
 * the balance of this node by a prior operation.
 *
 * Return the new balance of the subtree.
 */
static int sway(AvlNode **p, int sway)
{
	if ((*p)->balance != sway)
		(*p)->balance += sway;
	else
		balance(p, side(sway));
	
	return (*p)->balance;
}

/*
 * Perform tree rotations on an unbalanced node.
 *
 * side == 0 means the node's balance is -2 .
 * side == 1 means the node's balance is +2 .
 */
static void balance(AvlNode **p, int side)
{
	AvlNode  *node  = *p,
	         *child = node->lr[side];
	int opposite    = 1 - side;
	int bal         = bal(side);
	
	if (child->balance != -bal) {
		/* Left-left (side == 0) or right-right (side == 1) */
		node->lr[side]      = child->lr[opposite];
		child->lr[opposite] = node;
		*p = child;
		
		child->balance -= bal;
		node->balance = -child->balance;
		
	} else {
		/* Left-right (side == 0) or right-left (side == 1) */
		AvlNode *grandchild = child->lr[opposite];
		
		node->lr[side]           = grandchild->lr[opposite];
		child->lr[opposite]      = grandchild->lr[side];
		grandchild->lr[side]     = child;
		grandchild->lr[opposite] = node;
		*p = grandchild;
		
		node->balance       = 0;
		child->balance      = 0;
		
		if (grandchild->balance == bal)
			node->balance  = -bal;
		else if (grandchild->balance == -bal)
			child->balance = bal;
		
		grandchild->balance = 0;
	}
}


/************************* avl_check_invariants() *************************/

bool avl_check_invariants(AVL *avl)
{
	int    dummy;
	
	return checkBalances(avl->root, &dummy)
	    && checkOrder(avl)
	    && countNode(avl->root) == avl->count;
}

static bool checkBalances(AvlNode *node, int *height)
{
	if (node) {
		int h0, h1;
		
		if (!checkBalances(node->lr[0], &h0))
			return false;
		if (!checkBalances(node->lr[1], &h1))
			return false;
		
		if (node->balance != h1 - h0 || node->balance < -1 || node->balance > 1)
			return false;
		
		*height = (h0 > h1 ? h0 : h1) + 1;
		return true;
	} else {
		*height = 0;
		return true;
	}
}

static bool checkOrder(AVL *avl)
{
	AvlIter     i;
	const void *last     = NULL;
	bool        last_set = false;
	
	avl_foreach(i, avl) {
		if (last_set && avl->compare(last, i.key) >= 0)
			return false;
		last     = i.key;
		last_set = true;
	}
	
	return true;
}

static size_t countNode(AvlNode *node)
{
	if (node)
		return 1 + countNode(node->lr[0]) + countNode(node->lr[1]);
	else
		return 0;
}


/************************* Traversal *************************/

void avl_iter_begin(AvlIter *iter, AVL *avl, AvlDirection dir)
{
	AvlNode *node = avl->root;
	
	iter->stack_index = 0;
	iter->direction   = dir;
	
	if (node == NULL) {
		iter->key      = NULL;
		iter->value    = NULL;
		iter->node     = NULL;
		return;
	}
	
	while (node->lr[dir] != NULL) {
		iter->stack[iter->stack_index++] = node;
		node = node->lr[dir];
	}
	
	iter->key   = (void*) node->key;
	iter->value = (void*) node->value;
	iter->node  = node;
}

void avl_iter_next(AvlIter *iter)
{
	AvlNode     *node = iter->node;
	AvlDirection dir  = iter->direction;
	
	if (node == NULL)
		return;
	
	node = node->lr[1 - dir];
	if (node != NULL) {
		while (node->lr[dir] != NULL) {
			iter->stack[iter->stack_index++] = node;
			node = node->lr[dir];
		}
	} else if (iter->stack_index > 0) {
		node = iter->stack[--iter->stack_index];
	} else {
		iter->key      = NULL;
		iter->value    = NULL;
		iter->node     = NULL;
		return;
	}
	
	iter->node  = node;
	iter->key   = (void*) node->key;
	iter->value = (void*) node->value;
}
