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

#ifndef CCAN_BTREE_H
#define CCAN_BTREE_H

/*
Note:  The following should work but are not well-tested yet:

btree_walk...
btree_cmp_iters
btree_insert
btree_remove
btree_lookup
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/*
 * Maximum number of items per node.
 * The maximum number of branches is BTREE_ITEM_MAX + 1.
 */
#define BTREE_ITEM_MAX 20

struct btree_node {
	struct btree_node *parent;
	
	/* Number of items (rather than branches). */
	unsigned char count;
	
	/* 0 if node is a leaf, 1 if it has leaf children, etc. */
	unsigned char depth;
	
	/* node->parent->branch[node->k] == this */
	unsigned char k;
	
	const void *item[BTREE_ITEM_MAX];
	
	/*
	 * Allocated to BTREE_ITEM_MAX+1 items if this is
	 * an internal node, 0 items if it is a leaf.
	 */
	struct btree_node *branch[];
};

typedef struct btree_iterator_s {
	struct btree *btree;
	struct btree_node *node;
	unsigned int k;
	
	/*
	 * The relationship between item and (node, k) depends on what function
	 * set it.  It is mainly for convenience.
	 */
	void *item;
} btree_iterator[1];

/*
 * Instead of a compare function, this library accepts a binary search function
 * to know how to order the items.
 */
typedef unsigned int btree_search_proto(
	const void *key,
	const void * const *base,
	unsigned int count,
	int lr,
	int *found
);
typedef btree_search_proto *btree_search_t;

btree_search_proto btree_strcmp;

/*
 * Callback used by btree_delete() and btree_walk...().
 *
 * If it returns 0, it causes btree_walk...() to stop traversing and return 0.
 * Thus, in normal circumstances, this callback should return 1.
 *
 * Callback shall not insert/remove items from the btree being traversed,
 * nor shall anything modify it during a walk.
 */
typedef int (*btree_action_t)(void *item, void *ctx);

struct btree {
	struct btree_node *root;
	size_t count; /* Total number of items in B-tree */
	
	btree_search_t search;
	bool multi;
	
	/*
	 * These are set to NULL by default.
	 *
	 * When destroy is not NULL, it is called on each item in order when
	 * btree_delete() is called.
	 *
	 * When destroy is NULL, btree_delete runs faster because it does not have
	 * to visit each and every item.
	 */
	btree_action_t destroy;
	void *destroy_ctx;
};

struct btree *btree_new(btree_search_t search);
void btree_delete(struct btree *btree);

/* Inserts an item into the btree.  If an item already exists that is equal
 * to this one (as determined by the search function), behavior depends on the
 * btree->multi setting.
 *   If btree->multi is false (default), returns false, and no item
 *      is inserted (because it would be a duplicate).
 *   If btree->multi is true, returns true, putting the item after
 *      its duplicates.
 */
bool btree_insert(struct btree *btree, const void *item);

/* Removes an item from the btree.  If an item exists that is equal to the
 * key (as determined by the search function), it is removed.
 *
 * If btree->multi is set, all matching items are removed.
 *
 * Returns true if item was found and deleted, false if not found. */
bool btree_remove(struct btree *btree, const void *key);

/* Finds the requested item.
 * Returns the item pointer on success, NULL on failure.
 * Note that NULL is a valid item value.  If you need to put
 * NULLs in a btree, use btree_find instead. */
void *btree_lookup(struct btree *btree, const void *key);


/* lr must be 0 or 1, nothing else. */
int btree_begin_end_lr(const struct btree *btree, btree_iterator iter, int lr);
int btree_find_lr(const struct btree *btree, const void *key,
				btree_iterator iter, int lr);

int btree_walk_backward(const struct btree *btree,
				btree_action_t action, void *ctx);
int btree_walk_forward(const struct btree *btree,
				btree_action_t action, void *ctx);

#define btree_begin(btree, iter) btree_begin_end_lr(btree, iter, 0)
#define btree_end(btree, iter) btree_begin_end_lr(btree, iter, 1)

int btree_prev(btree_iterator iter);
int btree_next(btree_iterator iter);

#define btree_walk(btree, action, ctx) btree_walk_forward(btree, action, ctx)

/*
 * If key was found, btree_find_first will return 1, iter->item will be the
 * first matching item, and iter will point to the beginning of the matching
 * items.
 *
 * If key was not found, btree_find_first will return 0, iter->item will be
 * undefined, and iter will point to where the key should go if inserted.
 */
#define btree_find_first(btree, key, iter) btree_find_lr(btree, key, iter, 0)

/*
 * If key was found, btree_find_last will return 1, iter->item will be the
 * last matching item, and iter will point to the end of the matching
 * items.
 *
 * If key was not found, btree_find_last will return 0, iter->item will be
 * undefined, and iter will point to where the key should go if inserted.
 */
#define btree_find_last(btree, key, iter) btree_find_lr(btree, key, iter, 1)

/* btree_find is an alias of btree_find_first. */
#define btree_find(btree, key, iter) btree_find_first(btree, key, iter)

/*
 * If iter points to an item, btree_deref returns 1 and sets iter->item to the
 * item it points to.
 *
 * Otherwise (if iter points to the end of the btree), btree_deref returns 0
 * and leaves iter untouched.
 */
int btree_deref(btree_iterator iter);

/*
 * Inserts the item before the one pointed to by iter.
 *
 * Insertion invalidates all iterators to the btree, including the one
 * passed to btree_insert_at.  Nevertheless, iter->item will be set to
 * the item inserted.
 */
void btree_insert_at(btree_iterator iter, const void *item);

/*
 * Removes the item pointed to by iter.  Returns 1 if iter pointed
 * to an item.  Returns 0 if iter pointed to the end, in which case
 * it leaves iter intact.
 *
 * Removal invalidates all iterators to the btree, including the one
 * passed to btree_remove_at.  Nevertheless, iter->item will be set to
 * the item removed.
 */
int btree_remove_at(btree_iterator iter);

/*
 * Compares positions of two iterators.
 *
 * Returns -1 if a is before b, 0 if a is at the same position as b,
 * and +1 if a is after b.
 */
int btree_cmp_iters(const btree_iterator iter_a, const btree_iterator iter_b);

#define btree_search_implement(name, type, setup, equals, lessthan) \
unsigned int name(const void *__key, \
		const void * const *__base, unsigned int __count, \
		int __lr, int *__found) \
{ \
	unsigned int __start = 0; \
	while (__count) { \
		unsigned int __middle = __count >> 1; \
		type a = (type)__key; \
		type b = (type)__base[__start + __middle]; \
		{ \
			setup; \
			if (equals) \
				goto __equals; \
			if (lessthan) \
				goto __lessthan; \
		} \
	__greaterthan: \
		__start += __middle + 1; \
		__count -= __middle + 1; \
		continue; \
	__equals: \
		*__found = 1; \
		if (__lr) \
			goto __greaterthan; \
		/* else, fall through to __lessthan */ \
	__lessthan: \
		__count = __middle; \
		continue; \
	} \
	return __start; \
}

#endif /* #ifndef CCAN_BTREE_H */
