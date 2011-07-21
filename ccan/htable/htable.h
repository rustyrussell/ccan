/* Licensed under LGPLv2+ - see LICENSE file for details */
#ifndef CCAN_HTABLE_H
#define CCAN_HTABLE_H
#include "config.h"
#include <stdbool.h>
#include <stdlib.h>

struct htable;

/**
 * htable_new - allocate a hash tree.
 * @rehash: hash function to use for rehashing.
 * @priv: private argument to @rehash function.
 */
struct htable *htable_new(size_t (*hash)(const void *elem, void *priv),
			  void *priv);

/**
 * htable_free - dellocate a hash tree.
 *
 * This doesn't do anything to any pointers left in it.
 */
void htable_free(const struct htable *);

/**
 * htable_rehash - use a hashtree's rehash function
 * @elem: the argument to rehash()
 *
 */
size_t htable_rehash(const void *elem);

/**
 * htable_add - add a pointer into a hash tree.
 * @ht: the htable
 * @hash: the hash value of the object
 * @p: the non-NULL pointer
 *
 * Also note that this can only fail due to allocation failure.  Otherwise, it
 * returns true.
 */
bool htable_add(struct htable *ht, size_t hash, const void *p);

/**
 * htable_del - remove a pointer from a hash tree
 * @ht: the htable
 * @hash: the hash value of the object
 * @p: the pointer
 *
 * Returns true if the pointer was found (and deleted).
 */
bool htable_del(struct htable *ht, size_t hash, const void *p);

/**
 * struct htable_iter - iterator or htable_first or htable_firstval etc.
 *
 * This refers to a location inside the hashtable.
 */
struct htable_iter {
	size_t off;
};

/**
 * htable_firstval - find a candidate for a given hash value
 * @htable: the hashtable
 * @i: the struct htable_iter to initialize
 * @hash: the hash value
 *
 * You'll need to check the value is what you want; returns NULL if none.
 * See Also:
 *	htable_delval()
 */
void *htable_firstval(const struct htable *htable,
		      struct htable_iter *i, size_t hash);

/**
 * htable_nextval - find another candidate for a given hash value
 * @htable: the hashtable
 * @i: the struct htable_iter to initialize
 * @hash: the hash value
 *
 * You'll need to check the value is what you want; returns NULL if no more.
 */
void *htable_nextval(const struct htable *htable,
		     struct htable_iter *i, size_t hash);

/**
 * htable_get - find an entry in the hash table
 * @ht: the hashtable
 * @h: the hash value of the entry
 * @cmp: the comparison function
 * @ptr: the pointer to hand to the comparison function.
 *
 * Convenient inline wrapper for htable_firstval/htable_nextval loop.
 */
static inline void *htable_get(const struct htable *ht,
			       size_t h,
			       bool (*cmp)(const void *candidate, void *ptr),
			       const void *ptr)
{
	struct htable_iter i;
	void *c;

	for (c = htable_firstval(ht,&i,h); c; c = htable_nextval(ht,&i,h)) {
		if (cmp(c, (void *)ptr))
			return c;
	}
	return NULL;
}

/**
 * htable_first - find an entry in the hash table
 * @ht: the hashtable
 * @i: the struct htable_iter to initialize
 *
 * Get an entry in the hashtable; NULL if empty.
 */
void *htable_first(const struct htable *htable, struct htable_iter *i);

/**
 * htable_next - find another entry in the hash table
 * @ht: the hashtable
 * @i: the struct htable_iter to use
 *
 * Get another entry in the hashtable; NULL if all done.
 * This is usually used after htable_first or prior non-NULL htable_next.
 */
void *htable_next(const struct htable *htable, struct htable_iter *i);

/**
 * htable_delval - remove an iterated pointer from a hash tree
 * @ht: the htable
 * @i: the htable_iter
 *
 * Usually used to delete a hash entry after it has been found with
 * htable_firstval etc.
 */
void htable_delval(struct htable *ht, struct htable_iter *i);

#endif /* CCAN_HTABLE_H */
